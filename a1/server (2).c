#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <poll.h>

#define RECV_BUFFER_SIZE 24
#define SEND_BUFFER_SIZE 40
#define MAX_CLIENTS 10


struct server_arguments {
	int port;
	int percent_drop;
    int condensed;
};


typedef struct  {
    char ip_address[256]; 
    char port[256];
    int max_seq;
    time_t last_update;
} ClientInfo;

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
	case 'd':
		args->percent_drop = atoi(arg);
		break;
    case 'c':
		args->condensed = 1;
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void server_parseopt(struct server_arguments *args, int argc, char *argv[]) {

	/* bzero ensures that "default" parameters are all zeroed out */
	//bzero(&args, sizeof(args));

    bzero(args, sizeof(*args));

	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "drop", 'd', "drop", 0, "Percentage chance that the server drops", 0},
        { "condensed", 'c', "condensed", OPTION_ARG_OPTIONAL , "Use condensed message format", 0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		printf("Got an error condition when parsing\n");
	}

	/* What happens if you don't pass in parameters? Check args values
	 * for sanity and required parameters being filled in */

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got port %d and percent to be dropped %d, condensed=%d\n", args->port, args->percent_drop, args->condensed);
	 
}
 
void update_client(ClientInfo clients[], char *addr, char *port, int new_seq, time_t cur_time)
{
    printf("client addr=%s, port=%s, new_seq=%d, cur_time=%ld\n", addr, port,new_seq, cur_time);
    for(int i=0; i<MAX_CLIENTS; i++){

        if(!strcmp(clients[i].ip_address, addr) && !strcmp(clients[i].port, port)){     //new
            if(difftime(cur_time, clients[i].last_update) >= 120){  // 2 minutes
                clients[i].max_seq = new_seq;
                clients[i].last_update = cur_time;
            }else{
                if(clients[i].max_seq > new_seq){
                    printf("%s:%s %d %d\n", clients[i].ip_address, clients[i].port, new_seq, clients[i].max_seq);
                } else{
                    clients[i].max_seq = new_seq;
                    clients[i].last_update = cur_time;
                }
            }
        }else{
             if(difftime(cur_time, clients[i].last_update) >= 120){  // 2 minutes
                clients[i].max_seq = 0;
                clients[i].last_update = cur_time;
             }
        }
    }
}

int main(int argc, char *argv[]) {

    struct server_arguments args;
   
	server_parseopt(&args, argc, argv); //parse options as the server would
	
    fprintf(stderr, "%d\n", args.port);
    if(args.percent_drop){
       fprintf(stderr, "%d\n", args.percent_drop);
    }

    if(args.port <= 1024){
        fprintf(stderr, "You must use a port > 1024\n");
        exit(1);
    }

    if(args.percent_drop < 0 || args.percent_drop > 100){
        fprintf(stderr, "The value must be between 0 and 100\n");
        exit(1);
    }

    int sockfd;
    struct sockaddr_in server_addr;
    struct pollfd fds[MAX_CLIENTS];

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        fprintf(stderr, "socket");
        exit(1);
    }
    

    // Bind socket to port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(server_addr.sin_zero, sizeof(server_addr.sin_zero));

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr,"bind");
        exit(1);
    }

    printf("UDP server is running on port %d...\n", args.port);

    srand(time(NULL));
    socklen_t client_len = sizeof(struct sockaddr_storage);
    char client_addr_info[NI_MAXHOST] ={0};
    char client_port_info[NI_MAXSERV] ={0};

    // Initialize pollfd structures
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    int ret = 0;

    ClientInfo clients[MAX_CLIENTS];
    memset(clients, 0, MAX_CLIENTS*sizeof(ClientInfo));

    while(1){

        ret = poll(fds, 1, -1); // Wait indefinitely for data
        if (ret == -1) {
            fprintf(stderr, "poll error\n");
            exit(1);
        }

        // Check if socket has data to read
        if (fds[0].revents & POLLIN) {

            struct sockaddr_storage client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            
            uint8_t buffer[RECV_BUFFER_SIZE];  
            recvfrom(sockfd, buffer, RECV_BUFFER_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len); 

            getnameinfo((struct sockaddr *)&client_addr, client_addr_len, client_addr_info, sizeof(client_addr_info), client_port_info, sizeof(client_port_info), NI_NUMERICHOST | NI_NUMERICSERV);

            for(int n = 0; n < RECV_BUFFER_SIZE; n++)	printf("%02x",  buffer[n]);
            printf(" ... Received from client=%s, port=%s\n", client_addr_info, client_port_info);

            int random_number = rand() % 100;
            if(args.percent_drop < random_number){
                struct timespec tspec;
                clock_gettime(CLOCK_REALTIME,&tspec);
                time_t server_sec = tspec.tv_sec;
                long server_nanosec = tspec.tv_nsec;

                uint32_t client_seq;
                uint32_t client_ver;
                uint64_t client_sec;
                uint64_t client_nanosec;
                memcpy(&client_seq, buffer, 4);
                client_seq = ntohl(client_seq); 
                memcpy(&client_ver, buffer+4, 4);
                client_ver = ntohl(client_ver);             
                memcpy(&client_sec, buffer+8, 8);
                client_sec = be64toh(client_sec);
                memcpy(&client_nanosec,buffer+16,  8);
                client_nanosec = be64toh(client_nanosec);
                update_client(clients, client_addr_info, client_port_info, client_seq, server_sec);

                uint8_t buffer[SEND_BUFFER_SIZE];
                client_seq = htonl(client_seq); 
                client_ver = htonl(client_ver); 
                client_sec = htobe64(client_sec);
                client_nanosec = htobe64(client_nanosec);
                uint64_t serv_sec_nb = htobe64(server_sec);
                uint64_t serv_nanosec_nb = htobe64(server_nanosec);            
                memcpy(buffer, &client_seq, 4);
                memcpy(buffer+4, &client_ver, 4);
                memcpy(buffer+8, &client_sec, 8);
                memcpy(buffer+16, &client_nanosec, 8);
                memcpy(buffer+24, &serv_sec_nb, 8);
                memcpy(buffer+32, &serv_nanosec_nb, 8);
                
                sendto(sockfd, buffer, SEND_BUFFER_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
                for(int n = 0; n < SEND_BUFFER_SIZE; n++)	printf("%02x",  buffer[n]);printf(" ... Sent\n");



            }else{
                //printf("Dropped! args.drop=%d, random=%d \n", args.percent_drop , random_number);
            }
        }

    }


    close(sockfd);


}

