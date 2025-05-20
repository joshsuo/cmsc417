#include <argp.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <cassert>

#include "helloworld.pb.h"

using std::string;

#define PRINTLN(x) std::cout << x << "\n"
#define EPRINTLN(x) std::cerr << x << "\n"

static string HELLO_WORLD_STR("Hello, world!");
static string BYE_WORLD_STR("Bye, world!");

static struct argp_option options[] = 
{
	// see https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
	// long, short(key), associated arg desc (optional), flags, help msg, help group
	{ "port", 'p', "PORT", 0, "Port of the server", 0 },
	{ 0 } // sentinel
};

struct arguments
{
	uint16_t port;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = static_cast<struct arguments*>(state->input);
	switch (key)
	{
		case 'p':
			arguments->port = atoi(arg);
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
static struct argp argp = { options, parse_opt, 0, "Server that responds to string requests", 0, 0, 0 };

int main(int argc, char *argv[])
{
	int ret = 0;

	// -------- args parsing --------

	// put some defaults
	struct arguments args = 
	{
		.port = 0
	};

	// parse the command line arguments
	argp_parse(&argp, argc, argv, 0, 0, &args);

	// sancheck required arguments
	if (args.port == 0)
	{
		EPRINTLN("Port argument is required!");
		EPRINTLN("");
		// display help and exit
		// see https://www.gnu.org/software/libc/manual/html_node/Argp-Help.html
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, const_cast<char*>("server"));
		return 1;
	}

	// -------- socket calls --------

	// create a socket
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		EPRINTLN("Failed to create socket!");
		perror("socket");
		return 1;
	}

	// create bind address
	struct sockaddr_in bind_addr = {0};
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(args.port);
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind socket
	ret = bind(sock, (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in));
	if (ret < 0)
	{
		EPRINTLN("Failed to bind socket!");
		perror("bind");
		return 1;
	}

	// repeat forever
	while (true)
	{
		// allocate a buffer for the request
		uint8_t udp_recv_buf[65535] = {0};

		// create peer address struct
		struct sockaddr_in peer_addr = {0};
		socklen_t peer_addr_len = sizeof(struct sockaddr_in);

		// receive request
		ret = recvfrom(sock, udp_recv_buf, 65535, 0, (struct sockaddr *)&peer_addr, &peer_addr_len);
		if (ret < 0)
		{
			EPRINTLN("Failed to receive request!");
			perror("recvfrom");
			return 1;
		}

		// -------- parse request --------
		// unpack the received request
        StringRepeatRequest request;
		// check if unpacking succeeds
		if (!request.ParseFromArray(udp_recv_buf, ret))
		{
			EPRINTLN("Failed to unpack request!");
			continue;
		}

		// -------- process request --------
		// initialize the response struct (this is on stack)
		StringRepeatResponse response;
		string response_string; // declare these here because C++ bans gotos past initializations
		string response_buf;
		

		// sanity check the response struct, to see if we need to return an error
		// despite the protobuf definition says the number is int32, both the client and the server will treat it as int8
		// this is because protobuf lacks a native int8 type
		if ((int8_t)(request.repeat_count()) <= 0)
		{
			response.set_error(true);
			response.set_error_message("Repeat count must be positive!");

			goto err;
		}

		// key on the requested string
		switch (request.requested_string())
		{
			case HELLO_WORLD:
				for (int i = 0; i < request.repeat_count(); ++i)
				{
					response_string += HELLO_WORLD_STR;
				}
				break;
			case BYE_WORLD:
				for (int i = 0; i < request.repeat_count(); ++i)
				{
					response_string += BYE_WORLD_STR;
				}
				break;
			default:
				// this is still possible - for example, if the protobuf definition changes, and the client is built with the new definition, but the server is not
				response.set_error(true);
				response.set_error_message("Unknown requested string!");

				goto err;
		}

		// populate relevant fields on response
		response.set_error(false);
		response.set_repeated_string(response_string);

		// pack the message
		response.SerializeToString(&response_buf);

		// send the response
		ret = sendto(sock, response_buf.data(), response_buf.length(), 0, (struct sockaddr *)&peer_addr, peer_addr_len);
		if (ret < 0)
		{
			// server shouldn't crash
			EPRINTLN("Failed to send response!");
			perror("sendto");
			continue;
		}

		// udp sockets should send atomically
		assert((std::size_t)ret == response_buf.length());
		continue;

	// if we need to return an error
	err:
		// pack the message
		response.SerializeToString(&response_buf);
		
		// send the response
		ret = sendto(sock, response_buf.data(), response_buf.length(), 0, (struct sockaddr *)&peer_addr, peer_addr_len);
		if (ret < 0)
		{
			// server shouldn't crash
			EPRINTLN("Failed to send response!");
			perror("sendto");
			continue;
		}

		// udp sockets should send atomically
		assert((std::size_t)ret == response_buf.length());
		continue;
	}

	return 0;
}