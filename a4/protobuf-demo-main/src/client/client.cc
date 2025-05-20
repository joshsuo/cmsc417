#include <argp.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <time.h>

#include <iostream>
#include <string>
#include <cassert>

#include "helloworld.pb.h"

using std::string;

#define PRINTLN(x) std::cout << x << "\n"
#define EPRINTLN(x) std::cerr << x << "\n"

static struct argp_option options[] = 
{
	// see https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
	// long, short(key), associated arg desc (optional), flags, help msg, help group
	{ "server", 's', "IPv4_ADDRESS", 0, "IPv4 address of the server", 0 },
	{ "port", 'p', "PORT", 0, "Port of the server", 0 },
	{ "timeout", 't', "TIMEOUT_SEC", 0, "time to wait for the server (default: 10)", 0 },
	{ "display-string", 'd', "DISPLAY_STRING_IDX", 0, "Index of the string to request, as defined in the proto file (default: 0)", 0 },
	{ "repeat-count", 'r', "REPEAT_COUNT", 0, "Number of times to tell the server to repeat the string (default: 1)", 0 },
	{ 0 } // sentinel
};

struct arguments
{
	uint32_t server;
	uint16_t port;
	uint8_t timeout;
	DisplayString display_string;
	uint8_t repeat_count;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = static_cast<struct arguments*>(state->input);
	switch (key)
	{
		case 's':
			// to prevent confusion, all numbers in struct arguments are in host byte order
			arguments->server = ntohl(inet_addr(arg));
			break;
		case 'p':
			arguments->port = atoi(arg);
			break;
		case 't':
			arguments->timeout = atoi(arg);
			break;
		case 'd':
			arguments->display_string = static_cast<DisplayString>(atoi(arg));
			break;
		case 'r':
			arguments->repeat_count = atoi(arg);
			break;
		case ARGP_KEY_ARG:
			argp_usage(state);
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

// see https://www.gnu.org/software/libc/manual/html_node/Argp-Parsers.html
// options, parser, non_options_args_help, program description, argp_child[], help_filter(), 0
static struct argp argp = { options, parse_opt, 0, "Client that sends string request to server", 0, 0, 0 };

int main(int argc, char *argv[])
{
	int ret = 0;

	// -------- args parsing --------

	// put some defaults
	struct arguments args = 
	{
		.server = 0,
		.port = 0,
		.timeout = 10,
		.display_string = HELLO_WORLD,
		.repeat_count = 1
	};

	// parse the command line arguments
	argp_parse(&argp, argc, argv, 0, 0, &args);

	// sancheck required arguments
	if (args.server == 0 || args.port == 0)
	{
		EPRINTLN("Both server address and port are required!");
		EPRINTLN("");
		// display help and exit
		// see https://www.gnu.org/software/libc/manual/html_node/Argp-Help.html
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, const_cast<char*>("client"));
		return 1;
	}

	// -------- socket calls --------

	// create socket
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		EPRINTLN("Failed to create socket!");
		perror("socket");
		return 1;
	}

	// create server address
	struct sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(args.port);
	server_addr.sin_addr.s_addr = htonl(args.server);

	// -------- create request --------

	// initialize the request struct (this is on stack)
	StringRepeatRequest request;

	// set the request fields
	request.set_requested_string(args.display_string);
	request.set_repeat_count(args.repeat_count);

	// pack the message
	string request_buf = request.SerializeAsString();

	// -------- send --------
	ret = sendto(sock, request_buf.data(), request_buf.length(), 0, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
	if (ret < 0)
	{
		EPRINTLN("Failed to send request!");
		perror("sendto");
		return 1;
	}

	// udp sockets should send atomically
	assert((std::size_t)ret == request_buf.length());


	// -------- receive --------
	// get timestamp so we don't wait longer
	struct timespec start_time = {0};
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	// create buffer for UDP response
	uint8_t udp_recv_buf[65535] = {0};
	int actual_recv_len = 0;

	// repeat until we get a response from the server we're expecting
	while (true)
	{
		// -------- wait for response --------
		// create epoll instance (used for timeout)
		int epoll_fd = epoll_create1(0);
		if (epoll_fd < 0)
		{
			EPRINTLN("Failed to create epoll instance!");
			perror("epoll_create1");
			return 1;
		}

		// add socket to epoll instance
		struct epoll_event event = {0};
		event.events = EPOLLIN;
		event.data.fd = sock;
		ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event);
		if (ret < 0)
		{
			EPRINTLN("Failed to add socket to epoll instance!");
			perror("epoll_ctl");
			return 1;
		}

		// determine how long we have to wait
		struct timespec now = {0};
		clock_gettime(CLOCK_MONOTONIC, &now);
		int elapsed_ms = (now.tv_sec - start_time.tv_sec) * 1000 + (now.tv_nsec - start_time.tv_nsec) / 1000000;
		int timeout_ms = args.timeout * 1000 - elapsed_ms;
		if (timeout_ms <= 0)
		{
			EPRINTLN("Server timed out!");
			return 1;
		}

		// wait for response with timeout
		struct epoll_event events[1] = {0};
		ret = epoll_wait(epoll_fd, events, 1, timeout_ms);
		if (ret < 0)
		{
			EPRINTLN("Failed to wait for response!");
			perror("epoll_wait");
			return 1;
		}
		else if (ret == 0)
		{
			EPRINTLN("Server timed out!");
			return 1;
		}

		// -------- receive response --------

		// create structs for peer address
		struct sockaddr_in peer_addr = {0};
		socklen_t peer_addr_len = sizeof(struct sockaddr_in);

		// receive
		ret = recvfrom(sock, udp_recv_buf, 65535, 0, (struct sockaddr *)&peer_addr, &peer_addr_len);
		if (ret < 0)
		{
			EPRINTLN("Failed to receive response!");
			perror("recvfrom");
			return 1;
		}
		
		// check if the response is from the server we're expecting
		// otherwise, ignore it and wait for another response
		if (peer_addr.sin_addr.s_addr == server_addr.sin_addr.s_addr && peer_addr.sin_port == server_addr.sin_port)
		{
			actual_recv_len = ret;
			break;
		}
	}

	// -------- parse response --------
	// unpack the received response
	// this `malloc()`s under the hood! must be destroyed with `<message_name>__free_unpacked()`
	StringRepeatResponse response;
	
	// check if unpacking succeeds
	if (!response.ParseFromArray(udp_recv_buf, actual_recv_len))
	{
		EPRINTLN("Failed to parse response!");
		return 1;
	}

	// sancheck the response
	if (!response.has_repeated_string() && !response.has_error_message())
	{
		EPRINTLN("Response is missing fields!");
		return 1;
	}

	// check if server returned an error
	if (response.error())
	{
		EPRINTLN("Server returned an error:");
		EPRINTLN(response.error_message());
		return 1;
	}

	// print the response
	PRINTLN(response.repeated_string());

	return 0;
}