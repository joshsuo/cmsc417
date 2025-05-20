#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>

#define SEND_BUFFER_SIZE 24
#define RECV_BUFFER_SIZE 40
//#define _POSIX_C_SOURCE 200809L

struct client_arguments {
	char ip_address[256];  
	int port; /* is there already a structure you can store the address
	           * and port in instead of like this? */
	int req_num;
	int timeout;
    int condensed;
};

typedef struct  {
    uint32_t seq;
    float offset;
    float delay;
} response_status;

error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = state->input;
	error_t ret = 0;
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
		args->req_num = atoi(arg);
		break;
	case 't':
		/* validate argument makes sense */
		args->timeout = atoi(arg);
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


void client_parseopt(struct client_arguments *args,int argc, char *argv[]) {
    
    bzero(args, sizeof(*args));

	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "req_num", 'n', "req_num", 0, "The number of TimeRequests (N) that the client will send to the server", 0},
		{ "timeout", 't', "timeout", 0, " The time in seconds (T) that the client will wait after sending its last TimeRequest to receive a TimeResponse.", 0},
        { "condensed", 'c', "condensed", OPTION_ARG_OPTIONAL , "Use condensed message format", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0 };

	//struct client_arguments args;
	//bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		printf("Got error in parse\n");
	}

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	printf("Got %s on port %d with req_num=%d timeout=%d, condensed=%d\n",
	       args->ip_address, args->port, args->req_num, args->timeout, args->condensed);
 
}



int main(int argc, char *argv[]) {

    struct client_arguments args;
    bzero(&args, sizeof(args));
	client_parseopt(&args, argc, argv); //parse options as the client would
 
    if(args.req_num < 0){
        fprintf(stderr, "Num of TimeRequests must be greater than 0\n");
        abort();
    }
    if(args.port <= 1024){
        fprintf(stderr, "You must use a port more than 1024\n");
        abort();
    }

    struct sockaddr_in server_addr;  
    socklen_t server_len;
    struct pollfd fds[1];
    //struct timeval timeout;
    int poll_status;

    int client_sock = socket(AF_INET, SOCK_DGRAM, 0); // Socket descriptor for client
    if (client_sock < 0){
        fprintf(stderr, "socket failed\n");
        exit(1);
    }


    // Configure server address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args.port);
    server_addr.sin_addr.s_addr = inet_addr(args.ip_address);    
    server_len = sizeof(server_addr);


    // Initialize pollfd structure
    fds[0].fd = client_sock;
    fds[0].events = POLLIN;


    // responses status
    response_status responses[args.req_num];
    memset(responses, 0, args.req_num*sizeof(response_status));


     for(int i = 0; i < args.req_num; i++){
        uint8_t buffer[SEND_BUFFER_SIZE];
        uint32_t seq = i+1;
        uint32_t ver = 7;

        struct timespec tspec;

        clock_gettime(CLOCK_REALTIME, &tspec);
        time_t sec = tspec.tv_sec;
        long nanosec = tspec.tv_nsec;

        seq = htonl(seq);
        ver = htonl(ver);
        uint64_t sec_nb = htobe64(sec);
        uint64_t nanosec_nb = htobe64(nanosec);

        memcpy(buffer, &seq, 4);
        memcpy(buffer+4, &ver, 4);
        memcpy(buffer+8, &sec_nb, 8);
        memcpy(buffer+16, &nanosec_nb, 8);

        sendto(client_sock, buffer, SEND_BUFFER_SIZE ,0, (struct sockaddr *)&server_addr , server_len);
        
        for(int n = 0; n < SEND_BUFFER_SIZE; n++)	printf("%02x",  buffer[n]);
        printf(" ... Sent\n");

        // Wait for response using poll()
        poll_status = poll(fds, 1, args.timeout * 1000); // timeout
        if (poll_status == -1) {
            fprintf(stderr,"poll error\n");
            exit(1);
        } else if (poll_status == 0) {
            fprintf(stderr, "No response from server. Timeout occurred.\n");
            continue;
        }

        uint8_t recv_buffer[RECV_BUFFER_SIZE];

          // Receive message from server
        if (recvfrom(client_sock, recv_buffer, RECV_BUFFER_SIZE, 0, NULL, NULL) == -1) {
            fprintf(stderr,"recvfrom error\n");
            exit(1);
        }
        printf("Received: ");
        for(int n = 0; n < RECV_BUFFER_SIZE; n++)	printf("%02x",  recv_buffer[n]);
        printf("\n");



        uint32_t seq_recvd;
        uint32_t ver_recvd;;
        uint64_t client_sec_recvd;
        uint64_t client_nanosec_recvd;
        uint64_t serv_sec;
        uint64_t serv_nanosec;
        memcpy(&seq_recvd, recv_buffer,4);
        memcpy(&ver_recvd, recv_buffer+4,4);
        memcpy(&client_sec_recvd, recv_buffer+8,8);
        memcpy(&client_nanosec_recvd, recv_buffer+16,8);
        memcpy(&serv_sec, recv_buffer+24,8);
        memcpy(&serv_nanosec, recv_buffer+32,8);

        seq_recvd = ntohl(seq_recvd);
        ver_recvd = ntohl(ver_recvd);
        client_sec_recvd = be64toh(client_sec_recvd);
        client_nanosec_recvd = be64toh(client_nanosec_recvd);
        serv_sec = be64toh(serv_sec);
        serv_nanosec = be64toh(serv_nanosec);

        struct timespec tspec2;
        clock_gettime(CLOCK_REALTIME,&tspec2);
        time_t client_sec2 = tspec2.tv_sec;
        long client_nanosec2 = tspec2.tv_nsec;

        long double t0 = client_sec_recvd + client_nanosec_recvd/1000000000.0;
        long double t1 = serv_sec + serv_nanosec/1000000000.0;
        long double t2 = client_sec2 + client_nanosec2/1000000000.0;

        //printf("client_sec_recvd=%ld, client_nanosec_recvd=%ld\n", client_sec_recvd, client_nanosec_recvd);
        //printf("serv_sec=%ld, serv_nanosec=%ld\n", serv_sec, serv_nanosec);
        //printf("client_sec2=%ld, client_nanosec2=%ld\n", client_sec2, client_nanosec2);


        float offset = ((t1 - t0) + (t1 - t2)) / 2.0;
        float delay = t2 - t0;     

        responses[seq_recvd - 1].seq = seq_recvd;
        responses[seq_recvd - 1].offset = offset;
        responses[seq_recvd - 1].delay = delay;
        
     }


    close(client_sock);

     for(int i =0;i < args.req_num; i++){
        if(responses[i].seq == 0){
            printf("%d: Dropped\n", i+1);
        } else {
            printf("%d: %.4f %.4f\n", i+1, responses[i].offset, responses[i].delay);
        }
    }
 

}


