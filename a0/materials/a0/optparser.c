/* This is an example program that parses the options provided on the
 * command line that are needed for assignment 0. You may copy all or
 * parts of this code in the assignment */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>


struct server_arguments {
	int port;
	char *salt;
	size_t salt_len;
};


error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
	case 'p':
		/* Validate that port is correct and a number, etc!! */
		args->port = atoi(arg);
		if (0 /* port is invalid */) {
			argp_error(state, "Invalid option for a port, must be a number");
		}
		break;
	case 's':
		args->salt_len = strlen(arg);
		args->salt = malloc(args->salt_len+1);
		strcpy(args->salt, arg);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void server_parseopt(int argc, char *argv[]) {
	struct server_arguments args;

	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(&args, sizeof(args));



	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "salt", 's', "salt", 0, "The salt to be used for the server. Zero by default", 0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got an error condition when parsing\n");
	}

	/* What happens if you don't pass in parameters? Check args values
	 * for sanity and required parameters being filled in */

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got port %d and salt %s with length %ld\n", args.port, args.salt, args.salt_len);
	free(args.salt);
}

struct client_arguments {
	char ip_address[16]; /* You can store this as a string, but I probably wouldn't */
	int port; /* is there already a structure you can store the address
	           * and port in instead of like this? */
	int hashnum;
	int smin;
	int smax;
	char *filename; /* you can store this as a string, but I probably wouldn't */
};

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
	int len;
	switch(key) {
	case 'a':
		/* validate that address parameter makes sense */
		strncpy(args->ip_address, arg, 16);
		if (0 /* ip address is goofytown */) {
			argp_error(state, "Invalid address");
		}
		break;
	case 'p':
		/* Validate that port is correct and a number, etc!! */
		args->port = atoi(arg);
		if (0 /* port is invalid */) {
			argp_error(state, "Invalid option for a port, must be a number");
		}
		break;
	case 'n':
		/* validate argument makes sense */
		args->hashnum = atoi(arg);
		break;
	case 300:
		/* validate arg */
		args->smin = atoi(arg);
		break;
	case 301:
		/* validate arg */
		args->smax = atoi(arg);
		break;
	case 'f':
		/* validate file */
		len = strlen(arg);
		args->filename = malloc(len + 1);
		strcpy(args->filename, arg);
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}


void client_parseopt(int argc, char *argv[]) {
	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "hashreq", 'n', "hashreq", 0, "The number of hash requests to send to the server", 0},
		{ "smin", 300, "minsize", 0, "The minimum size for the data payload in each hash request", 0},
		{ "smax", 301, "maxsize", 0, "The maximum size for the data payload in each hash request", 0},
		{ "file", 'f', "file", 0, "The file that the client reads data from for all hash requests", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };

	struct client_arguments args;
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got error in parse\n");
	}

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got %s on port %d with n=%d smin=%d smax=%d filename=%s\n",
	       args.ip_address, args.port, args.hashnum, args.smin, args.smax, args.filename);
	free(args.filename);
}

int main(int argc, char *argv[]) {

	if (0) {
		server_parseopt(argc, argv); //parse options as the server would
	} else {
		client_parseopt(argc, argv); //parse options as the client would
	}
	return 0;
}
