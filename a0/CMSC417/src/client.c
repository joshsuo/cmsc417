#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

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

void client_parseopt(struct client_arguments *args, int argc, char *argv[]) {
    bzero(args, sizeof(*args));

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

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		printf("Got error in parse\n");
	}

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	//printf("Got %s on port %d with n=%d smin=%d smax=%d filename=%s\n",
	//       args->ip_address, args->port, args->hashnum, args->smin, args->smax, args->filename);
	//free(args.filename);
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

int main(int argc, char *argv[]) {

    struct client_arguments args;

	client_parseopt(&args, argc, argv); //parse options as the client would
 

    if(args.smax < args.smin){
        fprintf(stderr, "sman is greater than smax.\n");
        exit(EXIT_FAILURE);
    }

    if(args.hashnum < 0){
        fprintf(stderr, "The number of HashRequests (N) should be equal or greater than 0.\n");
        exit(EXIT_FAILURE);
    }

    if(args.smin < 1){
        fprintf(stderr, "The minimum size for the data payload should be equal or greater than 1.\n");
        exit(EXIT_FAILURE);
    }
    
    if(args.smax  > pow(2, 24) ){
        fprintf(stderr, "The maximum size for the data payload should be equal or smaller than 2^24.\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(args.filename, "/dev/zero") != 0) {
        struct stat st;
        if (stat(args.filename, &st) == 0) {
            if(st.st_size < args.hashnum * args.smax){
                fprintf(stderr, "File is too small. File size: %ld bytes... Should be greater than %d bytes\n", st.st_size, args.hashnum * args.smax);
                exit(EXIT_FAILURE);
            }
        }else{
            fprintf(stderr,"File doesn't exist or cannot get file size.\n");
            exit(EXIT_FAILURE);
        }
    }




    int sockfd;
    //Create a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        //perror("\n Socket creation error \n");
        fprintf(stderr, "Socket creation error.\n");
        return -1;
    }
    
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(args.port);

    // Convert IPv4 and IPv6 addresses from text to binary
    if (inet_pton(AF_INET, args.ip_address, &server_addr.sin_addr.s_addr) <= 0) {
        //perror("Invalid address/ Address not supported \n");
        fprintf(stderr, "Invalid address/ Address not supported \n");
        return -1;
    }

    //2. Establish a connection
    int status;
    if ((status = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0) {
        //printf("\nConnection Failed \n");
        fprintf(stderr, "\nConnection Failed \n");
        return -1;
    }
    // 3. Communicate
    uint32_t type = htonl(1);
    uint32_t num_req = htonl(args.hashnum);
    uint32_t total_len = 0;


    // initialization
    send_data(&type, 4, sockfd);
    send_data(&num_req, 4, sockfd);

    //achknowledgement
    read_data(&type, 4, sockfd);
    read_data(&total_len, 4, sockfd);
	type = ntohl(type);
	total_len = ntohl(total_len);


    FILE *file = fopen (args.filename , "r");
	 

	//srand(time(NULL));	
    for(int i = 0; i < args.hashnum; i++){
        uint32_t l =  ((rand()%(args.smax-args.smin+1)) + args.smin);
		size_t cur_len = 0;
        uint8_t send_buffer[l];

		while(cur_len < l){
			size_t bytes_remain = l - cur_len;
			size_t bytes_read = fread(send_buffer + cur_len, 1, bytes_remain, file);
			cur_len += bytes_read;
		}
		
		uint32_t hashreq_type = htonl(3);
		uint32_t random_len_nb = htonl(l);
		send_data(&hashreq_type, 4, sockfd);
		send_data(&random_len_nb, 4, sockfd);
        send_data(send_buffer, l, sockfd);

        uint8_t info_buffer[8] = {0};
        uint8_t receive_buffer[33] = {0};

        read_data(info_buffer, 8, sockfd);
        read_data(receive_buffer, 32, sockfd);
		
		printf("%d: 0x", i+1);
		for(int n = 0; n < 32; n++)	printf("%02x",  receive_buffer[n]);
		
		putchar('\n');
        
    }

    fclose(file);
    close(sockfd);
 

	return 0;
}