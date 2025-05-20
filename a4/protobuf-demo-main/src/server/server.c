#include <argp.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helloworld.pb-c.h"

#define PRINTLN(x) printf("%s\n", x)
#define EPRINTLN(x) fprintf(stderr, "%s\n", x)

static char* HELLO_WORLD_STR = "Hello, world!";
static char* BYE_WORLD_STR = "Bye, world!";

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
	struct arguments *arguments = state->input;
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
		argp_help(&argp, stderr, ARGP_HELP_STD_HELP, "server");
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
		// this `malloc()`s under the hood! must be destroyed with `<message_name>__free_unpacked()`
		StringRepeatRequest *request = string_repeat_request__unpack(NULL, ret, udp_recv_buf);

		// check if unpacking was successful
		if (request == NULL)
		{
			EPRINTLN("Failed to unpack request!");
			continue;
		}

		// -------- process request --------
		// initialize the response struct (this is on stack)
		StringRepeatResponse response = STRING_REPEAT_RESPONSE__INIT;

		// sanity check the response struct, to see if we need to return an error
		// despite the protobuf definition says the number is int32, both the client and the server will treat it as int8
		// this is because protobuf lacks a native int8 type
		if ((int8_t)(request->repeat_count) <= 0)
		{
			response.error = true;
			response.error_message = "Repeat count must be positive!";

			goto err;
		}

		// we need to allocate the response string, so relevant variables are populated here
		int response_string_len = 0;
		char *response_string = NULL;

		// key on the requested string
		switch (request->requested_string)
		{
			case DISPLAY_STRING__HELLO_WORLD:
				response_string_len = strlen(HELLO_WORLD_STR) * request->repeat_count;
				response_string = malloc(response_string_len + 1);
				memset(response_string, 0, response_string_len + 1);
				for (int i = 0; i < request->repeat_count; i++)
				{
					strcat(response_string, HELLO_WORLD_STR);
				}
				break;
			case DISPLAY_STRING__BYE_WORLD:
				response_string_len = strlen(BYE_WORLD_STR) * request->repeat_count;
				response_string = malloc(response_string_len + 1);
				memset(response_string, 0, response_string_len + 1);
				for (int i = 0; i < request->repeat_count; i++)
				{
					strcat(response_string, BYE_WORLD_STR);
				}
				break;
			default:
				// this is still possible - for example, if the protobuf definition changes, and the client is built with the new definition, but the server is not
				response.error = true;
				response.error_message = "Unknown requested string!";

				goto err;
		}

		// populate relevant fields on response
		response.error = false;
		response.repeated_string = response_string;

		// pack the message
		size_t response_len = string_repeat_response__get_packed_size(&response);
		uint8_t *response_buf = malloc(response_len);

		// pack, sizes should match
		assert(response_len == string_repeat_response__pack(&response, response_buf));

		// send the response
		ret = sendto(sock, response_buf, response_len, 0, (struct sockaddr *)&peer_addr, peer_addr_len);
		if (ret < 0)
		{
			// server shouldn't crash
			EPRINTLN("Failed to send response!");
			perror("sendto");
			string_repeat_request__free_unpacked(request, NULL);
			free(response_buf);
			free(response_string);
			continue;
		}

		// udp sockets should send atomically
		assert(ret == response_len);

		string_repeat_request__free_unpacked(request, NULL);
		free(response_buf);
		free(response_string);
		continue;

	// if we need to return an error
	err:
		// pack the message
		response_len = string_repeat_response__get_packed_size(&response);
		response_buf = malloc(response_len);
		
		// pack, sizes should match
		assert(response_len == string_repeat_response__pack(&response, response_buf));

		// send the response
		ret = sendto(sock, response_buf, response_len, 0, (struct sockaddr *)&peer_addr, peer_addr_len);
		if (ret < 0)
		{
			// server shouldn't crash
			EPRINTLN("Failed to send response!");
			perror("sendto");
			string_repeat_request__free_unpacked(request, NULL);
			free(response_buf);
			continue;
		}

		// udp sockets should send atomically
		assert(ret == response_len);

		string_repeat_request__free_unpacked(request, NULL);
		free(response_buf);
		continue;
	}

	return 0;
}