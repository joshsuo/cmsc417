#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netinet/in.h>
//#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

#include "hash.h"
#define MAX_CLIENT_QUEUE 10


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

void server_parseopt(struct server_arguments *args, int argc, char *argv[]) {

	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(args, sizeof(*args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "salt", 's', "salt", 0, "The salt to be used for the server. Zero by default", 0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		printf("Got an error condition when parsing\n");
	}

    // if (!args->port) {
	// 	fputs("A port number must be specified\n", stderr);
	// 	exit(1);
	// }

	/* What happens if you don't pass in parameters? Check args values
	 * for sanity and required parameters being filled in */

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got port %d and salt %s with length %ld\n", args->port, args->salt, args->salt_len);
	//free(args.salt);

}


void send_data(void *buffer, size_t bytes_expected, int send_socket){
    size_t bytes_sent = 0;
    size_t temp = 0;

    while(bytes_sent < bytes_expected){
        temp = send(send_socket, buffer + bytes_sent, bytes_expected-bytes_sent, 0);
        bytes_sent += temp;
    }
}

void read_data(void *buffer, size_t bytes_expected, int read_socket){
    size_t bytes_received = 0;
    size_t temp = 0;

    while(bytes_received < bytes_expected){
        temp = recv(read_socket, buffer + bytes_received, bytes_expected-bytes_received, MSG_WAITALL);
        bytes_received += temp;
    }

}

int main(int argc, char *argv[])
{
    // 1. Create a TCP socket
    // 2. Bind socket to a port
    // 3. Set socket to listen
    // 4. Repeatedly:
    //  a. Accept new connection
    //  b. Communicate
    //  c. Close the connection
    struct server_arguments args;

    //printf("I am the server.\n");

    server_parseopt(&args, argc, argv);

    // 1. Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) {
        fprintf(stderr, "Server Socket Failed\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(args.port);

    // 2. Bind socket to a port
    //printf("%d\n",args.port);
    int sockbind = bind(sockfd, (struct sockaddr*)&address, sizeof(address));
    if(sockbind < 0) {
        fprintf(stderr, "Bind Failed\n");
        exit(EXIT_FAILURE);
    }

    // 3. Set socket to listen
    if(listen(sockfd, MAX_CLIENT_QUEUE) < 0) {
        fprintf(stderr, "Listen Failed\n");
        exit(EXIT_FAILURE);
    }

    // 4. Repeatedly:
    //  a. Accept new connection
    //  b. Communicate
    //  c. Close the connection
    //int new_sock;
    //socklen_t addrlen = sizeof(address);
    //ssize_t num_bytes;
    //char buffer[1024] = {0};
    //char* hello = "Hello from server";

    while(1){

        // Add client
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_socket;

        if ((client_socket  = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen))  < 0) {
                //perror("accept failed");
                fprintf(stderr, "accept() failed\n");
                //exit(EXIT_FAILURE);
                continue;
        }    



            // Receive Initialization

            uint32_t type;
            read_data(&type, 4, client_socket);
            uint32_t hashreq_num;
            read_data(&hashreq_num, 4, client_socket);
            fprintf(stderr, "Server will get %d hash Requests\n", ntohl(hashreq_num));



            // Send  Acknowledgement
            int total_len = ntohl(hashreq_num);
            uint32_t type_nb = htonl(2);
            send_data(&type_nb, 4, client_socket);
            send_data(&hashreq_num, 4, client_socket);


            uint32_t counter;
            uint32_t mes_length;
            struct checksum_ctx *ctx;
            if(args.salt_len == 0){
                ctx = checksum_create(NULL, 0);
            }else{
                ctx = checksum_create(((uint8_t *)args.salt), sizeof(args.salt)-1);
            }

            if (!ctx) {
                fprintf(stderr, "Error creating checksum\n");
                return 0;
            }
            
            
 
            for(counter = 0; counter < htonl(hashreq_num); counter++){
                //Hash request
                read_data(&type, 4, client_socket);
                read_data(&mes_length, 4, client_socket);
                mes_length = ntohl(mes_length);

                void *read_buffer = NULL;
                int update_times= 0;
                if(mes_length <= UPDATE_PAYLOAD_SIZE){
                    read_buffer = calloc(1, mes_length);
                    read_data(read_buffer, mes_length, client_socket);
                }else{
                    read_buffer = calloc(1, UPDATE_PAYLOAD_SIZE);
                    int remainder = mes_length % UPDATE_PAYLOAD_SIZE;
                    update_times = (mes_length-remainder)/UPDATE_PAYLOAD_SIZE;
                    for(int i = 0; i < update_times; i++){
                        read_data(read_buffer, UPDATE_PAYLOAD_SIZE, client_socket);
                        checksum_update(ctx, read_buffer);
                    }
                    free(read_buffer);
                    read_buffer = calloc(1,remainder);
                    read_data(read_buffer, remainder, client_socket);
                }
                
                uint8_t checksum[32];
                checksum_finish(ctx, read_buffer, mes_length-UPDATE_PAYLOAD_SIZE*update_times, checksum);
                checksum_reset(ctx);

                //Hash response
                uint32_t counter_nb = htonl(counter);
                type_nb = htonl(4);
                send_data(&type_nb, 4, client_socket);
                send_data(&counter_nb, 4, client_socket);
                send_data(checksum, 32, client_socket);

                free(read_buffer);
            }
             
            checksum_destroy(ctx);
             close(client_socket);     
         
        
    }

    close(sockfd);

    return 0;
}