#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <argp.h>
#include <unistd.h>
#include <time.h>
#include <netdb.h>
#include <math.h>

#define SEND_BUFFER_SIZE 24
#define RECV_BUFFER_SIZE 40

struct client_arguments {
	char ip_address[16];
	int port; 
	int reqnum;
	int timeout;
    int condensed;
};

typedef struct {
    uint32_t seq;
    float theta;
    float delta;
} Response ;

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
		args->reqnum = atoi(arg);
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
		{ "ip_address", 'a', "addr", 0, "The IP address the server is listening at", 0},
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
	       args->ip_address, args->port, args->reqnum, args->timeout, args->condensed);
 
}

int main(int argc, char *argv[]){

    struct client_arguments args;
    bzero(&args, sizeof(args));
	client_parseopt(&args, argc, argv);
 
    if(!strcmp("", args.ip_address)){
        fprintf(stderr, "IP addr must be specified\n");
        abort();
    }
    if(args.reqnum < 0){
        fprintf(stderr, "Num of TimeRequests must >=0\n");
        abort();
    }
    if(args.port <= 1024){
        fprintf(stderr, "You must use a port > 1024\n");
        abort();
    }
    
    
    fprintf(stderr, "Got %s on port %d with reqnum=%d timeout=%d\n",
        args.ip_address, args.port, args.reqnum, args.timeout);
           
    struct addrinfo addrCriteria; // Criteria for address match
    memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
    addrCriteria.ai_family = AF_UNSPEC; // Any address family

    addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram sockets
    addrCriteria.ai_protocol = IPPROTO_UDP; // Only UDP protocol

    struct addrinfo *serv_addr; // List of server addresses
    uint8_t port_str[1025] = {0};
    sprintf(port_str, "%d", args.port);
    int rtnVal = getaddrinfo(args.ip_address, port_str, &addrCriteria, &serv_addr);
    if (rtnVal != 0)
        fprintf(stderr, "getaddrinfo failed\n");

    int sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol); // Socket descriptor for client
    if (sock < 0)
        fprintf(stderr, "socket failed\n");

    for(int i = 0; i < args.reqnum; i++){

        uint8_t buffer[SEND_BUFFER_SIZE + (args.condensed ? -2 : 0)]; // if condesed, should be 22
        
        struct timespec tspec;
        clock_gettime(CLOCK_REALTIME,&tspec);
        time_t sec = tspec.tv_sec;
        long nanosec = tspec.tv_nsec;

        uint32_t seq = i+1;
        seq = htonl(seq);
        uint64_t sec_nb = htobe64(sec);
        uint64_t nanosec_nb = htobe64(nanosec);

        memcpy(buffer, &seq, 4);

        if(args.condensed) {
            uint16_t ver = htons(7);
            memcpy(buffer+4, &ver, 2);
            memcpy(buffer+6, &sec_nb, 8);
            memcpy(buffer+14, &nanosec_nb, 8);
        } else {
            uint32_t ver = htonl(7);
            memcpy(buffer+4, &ver, 4);
            memcpy(buffer+8, &sec_nb, 8);
            memcpy(buffer+16, &nanosec_nb, 8);
        }
        // 24 => 22, if condensed
        sendto(sock, buffer, SEND_BUFFER_SIZE + (args.condensed ? -2 : 0) ,0, serv_addr->ai_addr, serv_addr->ai_addrlen); 

    }

    struct timeval timeout={args.timeout,0}; 
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
    struct sockaddr_storage fromAddr; // Source address of server
    socklen_t fromAddrLen = sizeof(fromAddr);

    Response responses[args.reqnum];
    memset(responses, 0, args.reqnum*sizeof(Response));
    
    int buffer_len = RECV_BUFFER_SIZE + (args.condensed ? -2 : 0);
    for(int i = 0; i < args.reqnum; i++){
        uint8_t buffer[buffer_len]; // buffer[40] => [38], if condensed
        int recvlen = recvfrom(sock, buffer, buffer_len, 0, (struct sockaddr *)&fromAddr, &fromAddrLen);
        if (recvlen >= 0) {

            struct timespec tspec;
            clock_gettime(CLOCK_REALTIME,&tspec);
            time_t client_sec2 = tspec.tv_sec;
            long client_nanosec2 = tspec.tv_nsec;

            uint32_t seq;
            uint64_t client_sec1;
            uint64_t client_nanosec1;
            uint64_t serv_sec;
            uint64_t serv_nanosec;

            if(args.condensed) {
                uint16_t ver;
                memcpy(&seq, buffer,4);
                memcpy(&ver, buffer+4,2);
                memcpy(&client_sec1, buffer+6,8);            
                memcpy(&client_nanosec1, buffer+14,8);
                memcpy(&serv_sec, buffer+22,8);
                memcpy(&serv_nanosec, buffer+30,8);
                ver = ntohs(ver);
            } else {
                uint32_t ver;
                memcpy(&seq, buffer,4);
                memcpy(&ver, buffer+4,4);
                memcpy(&client_sec1, buffer+8,8);            
                memcpy(&client_nanosec1, buffer+16,8);
                memcpy(&serv_sec, buffer+24,8);
                memcpy(&serv_nanosec, buffer+32,8);
                ver = ntohl(ver);
            }
           
            seq = ntohl(seq);
            client_sec1 = be64toh(client_sec1);
            client_nanosec1 = be64toh(client_nanosec1);
            serv_sec = be64toh(serv_sec);
            serv_nanosec = be64toh(serv_nanosec);

            if(responses[seq-1].seq != 0){
                //Duplicate TimeResponse
                i--;
            }else{
                long double t0 = client_sec1 + client_nanosec1/1000000000.0;
                long double t1 = serv_sec + serv_nanosec/1000000000.0;
                long double t2 = client_sec2 + client_nanosec2/1000000000.0;
        
                float theta = (t1-t0 + t1 - t2)/2;
                float delta = t2 - t0;
                responses[seq-1].seq = seq;
                responses[seq-1].delta = delta;
                responses[seq-1].theta = theta;
            }
        }
        else{
            break;
        }
    }

    close(sock);

    for(int i =0;i < args.reqnum; i++){
        if(responses[i].seq == 0)
            printf("%d: Dropped\n", i+1);
        else
            printf("%d: %.4f %.4f\n", i+1, responses[i].theta, responses[i].delta);
    }
}