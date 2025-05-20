#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/time.h>

#include "chord_arg_parser.h"
#include "chord.h"
#include "hash.h"

#define MAX_CLIENTS 8
#define MAX_LINE 100
#define MAX_FT 64


struct node_info {
	Node *node;
	Node *successor;
	Node *predecessor;

};


 void DieWithSystemMessage(const char *msg) {
  fprintf(stderr, msg);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
 }
 void show_error_msg(const char *msg) {
	  fprintf(stderr, msg);
	   fprintf(stderr, "\n");
	  return;
 }


int read_bytes(int sockfd, void *buffer, int bytes){
    int bytes_received = 0;
    int temp = 0;

    while(bytes_received < bytes){
        temp = recv(sockfd, buffer + bytes_received, bytes-bytes_received, 0);
        if(temp == -1){
			show_error_msg("read failed");
            return -1;
        }else if(temp ==0){
			return 0;
		}
        bytes_received += temp;
    }
	return bytes_received;
}



void send_bytes(int sockfd,  void *buffer, int bytes){
    int bytes_sent = 0;
    int temp = 0;

    while(bytes_sent < bytes){
        temp = send(sockfd, buffer + bytes_sent, bytes-bytes_sent, 0);
        if(temp == -1){
			show_error_msg("send failed");
            break;
        }
        bytes_sent += temp;
    }
}

void copy_node(Node *n0, Node *n1) {
    n0->address = n1->address;
    n0->port = n1->port;
    n0->key = n1->key;
}

uint64_t hash_addr(struct sockaddr_in addr) {
    
	char address[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(addr.sin_addr.s_addr), address, INET_ADDRSTRLEN) == NULL) {
        DieWithSystemMessage("inet_ntop() failed");
    }

	struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

    // Convert port to string
    char port[6] = {0};
    sprintf(port, "%d", ntohs(addr.sin_port));


    char *payload = malloc(strlen(address) + strlen(port) + 2);
    strcpy(payload, address);
    strcat(payload, ":");
    strcat(payload, port);


    uint8_t checksum[20];

    int error = sha1sum_finish(ctx, (const uint8_t *)payload, strlen(payload), checksum);
    if (error != 0) {
        perror("checksum finish error");
        exit(-1);
    }

    uint64_t ret = sha1sum_truncated_head(checksum);
    sha1sum_reset(ctx);
	sha1sum_destroy(ctx);
    free(payload);
    return ret;
}



int listen_clients(struct sockaddr_in *address) {

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		DieWithSystemMessage("socket() failed");
    }

    // Bind to the local address
    if (bind(sockfd, (struct sockaddr *) address, sizeof(struct sockaddr_in)) < 0) {
		DieWithSystemMessage("bind() failed");
    }

    // Listen
    if (listen(sockfd, SOMAXCONN) < 0) {
		DieWithSystemMessage("listen() failed");
    }
    return sockfd;
}




// send the ChordMessage
void send_message(int sockfd, const ChordMessage *msg) {
    // pack
	
    size_t len = chord_message__get_packed_size(msg);

    void *buf = malloc(len);
    chord_message__pack(msg, buf);

    // Send
    uint64_t length_to_send = htobe64((uint64_t)len);


    ssize_t retval = send(sockfd, &length_to_send, sizeof(uint64_t), 0);
    if (retval < 0) {
        free(buf);
		DieWithSystemMessage("send() failed");
    }
    assert(retval == sizeof(uint64_t));

    retval = send(sockfd, buf, len, 0);
    if (retval < 0) {
        free(buf);
		DieWithSystemMessage("send() failed");
    }
    assert((size_t)retval == len);

    free(buf);
}

// returned the recieved ChordMessage
ChordMessage *recv_message(int sockfd ){
    // get succ
    uint64_t msg_len = 0;
	read_bytes(sockfd, &msg_len, sizeof(u_int64_t));

    msg_len = be64toh(msg_len);

    ChordMessage *resp;
    uint8_t *resp_buf = malloc(msg_len);

    ssize_t retval = recv(sockfd, resp_buf, msg_len, 0);
    if (retval <= 0) {
		show_error_msg("recv error");
        return NULL;
    } else if (retval == 0) {
		show_error_msg("recv() failed, connection closed prematurely");
        return NULL;
    }

    resp = chord_message__unpack(NULL, msg_len, resp_buf);
    if (resp == NULL) {
        perror("Failed to parse response\n");
        exit(-1);
    }

    free(resp_buf);

    return resp;
}

Node *closest_preceding_node(Node *n, Node ft[], uint64_t id) {
    int i;
    Node *ret = malloc(sizeof(Node));
    for (i = MAX_FT; i > 1; i--) {
        if (ft[i].key > n->key && ft[i].key < id) {
            ret->address = ft[i].address;
            ret->port = ft[i].port;
            ret->key = ft[i].key;
            return ret;
        }
    }
    return n;
}



Node *find_successor(Node *n, Node *succ, Node *pred, Node ft[], uint64_t id) {

    if (pred != NULL && id > pred->key && id <= n->key) {
        return n;

    } else if (succ != NULL && id > n->key && id <= succ->key) {
        return succ;

    } else {
        Node *next_node = closest_preceding_node(n, ft, id);

		if(next_node == n){
			return succ;
		}

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd < 0) {
			DieWithSystemMessage("socket() failed");
        }

        struct sockaddr_in addr;
        if (succ == n) {
            // if there is only the one node in the chord so far
            return n;
        } else {
                       
            addr.sin_addr.s_addr = next_node->address;
            addr.sin_port = htons(next_node->port);
        }
        addr.sin_family = AF_INET;

        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
			DieWithSystemMessage("connect() failed");
        }

        free(next_node);

        // Initialize request struct and set key
        ChordMessage msg = CHORD_MESSAGE__INIT;
		msg.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST;

		FindSuccessorRequest req = FIND_SUCCESSOR_REQUEST__INIT;
        Node node_to_send = NODE__INIT;

        copy_node(&node_to_send, n);
        req.requester = &node_to_send;
        req.key = id;
		msg.find_successor_request = &req;

        // send and recv
        send_message(sockfd, &msg);
        ChordMessage *resp = recv_message(sockfd);

        Node *new_succ = malloc(sizeof(Node));
        copy_node(new_succ, resp->find_successor_response->node);
        chord_message__free_unpacked(resp, NULL);
        return new_succ;

    }
}


void init_node(Node *node, struct sockaddr_in addr) {

    node__init(node);
    node->address = addr.sin_addr.s_addr;
    node->port = ntohs(addr.sin_port);
    node->key = hash_addr(addr);
}

Node *join(Node *n, struct sockaddr_in n_prime_addr) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
		DieWithSystemMessage("socket() failed");
    }

    if (connect(sockfd, (struct sockaddr *)&n_prime_addr, sizeof(struct sockaddr_in)) < 0) {
		DieWithSystemMessage("connect() failed");
    }

    ChordMessage msg = CHORD_MESSAGE__INIT;
	msg.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST;

	FindSuccessorRequest req = FIND_SUCCESSOR_REQUEST__INIT;

    Node node_to_send = NODE__INIT;

    copy_node(&node_to_send, n);
    req.requester = &node_to_send;
    req.key = n->key;    
	msg.find_successor_request = &req;

    // send
    send_message(sockfd, &msg);
    // recv
    ChordMessage *resp = recv_message(sockfd);

    Node *new_succ = malloc(sizeof(Node));
	copy_node(new_succ, resp->find_successor_response->node);
    uint64_t new_key = resp->find_successor_response->key;
	chord_message__free_unpacked(resp, NULL);
    return new_succ;
}





void stabilize(Node sl[], int len, Node *successor, Node *my_node) {

    Node *x = malloc(sizeof(Node));

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
		DieWithSystemMessage("socket() failed");
    }

    for (int i = 0; i < len; i++) {
        struct sockaddr_in addr;
        addr.sin_port = htons(sl[i].port);
        addr.sin_addr.s_addr = sl[i].address;
        addr.sin_family = AF_INET;

        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
            if (i == len - 1) {
                return;
            }
            continue;
        }
        break;
    }

    // get successor's pred
    ChordMessage msg = CHORD_MESSAGE__INIT;
    msg.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST;
    GetPredecessorRequest pred_req = GET_PREDECESSOR_REQUEST__INIT;
    msg.get_predecessor_request = &pred_req;

    // send
    send_message(sockfd, &msg);
    ChordMessage *resp = recv_message(sockfd);

    // x is the pred
    copy_node(x, resp->get_predecessor_response->node);

    chord_message__free_unpacked(resp, NULL);

    // update succ if needed
    if (my_node->key < x->key && x->key <= successor->key) {
        copy_node(successor, x);
    }
    free(x);

    ChordMessage msg2 = CHORD_MESSAGE__INIT;
    msg2.msg_case = CHORD_MESSAGE__MSG_NOTIFY_REQUEST;
    NotifyRequest notify_req = NOTIFY_REQUEST__INIT;
    Node node_to_send = NODE__INIT;
    copy_node(&node_to_send, my_node);
    notify_req.node = &node_to_send;
    msg2.notify_request = &notify_req;

    send_message(sockfd, &msg2);
}




void fix_fingers(Node *node, Node *succ, Node *pred, Node ft[], int *next) {

	*next = *next +1;

	if(*next > MAX_FT){
		*next = 1;
	}

    int i  = *next;
    Node *n = malloc(sizeof(Node));
	uint64_t key = node->key + (i == 0 ? 0 : (uint64_t)(1 << (i -1))) ; 
	n = find_successor(node, succ, pred, ft, key);
	ft[i].address = n->address;
	ft[i].port = n->port;
	ft[i].key = n->key;

    free(n);
}





void check_predecessor(Node *pred) {
    // If pred is already NULL return
    if (pred == NULL) {
        return;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        DieWithSystemMessage("socket() failed");
    }

    // construct pred_addr to connect with
    struct sockaddr_in pred_addr;
    pred_addr.sin_port = htons(pred->port);
    pred_addr.sin_addr.s_addr = pred->address;
    pred_addr.sin_family = AF_INET;

    if (connect(sockfd, (struct sockaddr *)&pred_addr, sizeof(struct sockaddr_in)) < 0) {
        // connection failed, set pred to NULL and return
        pred = NULL;
        return;
    }

    // Build ChordMessage to send
    ChordMessage msg = CHORD_MESSAGE__INIT;
    msg.msg_case = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST;
    CheckPredecessorRequest req = CHECK_PREDECESSOR_REQUEST__INIT;
    msg.check_predecessor_request = &req;

    send_message(sockfd, &msg);

    ChordMessage *return_msg = recv_message(sockfd);

    chord_message__free_unpacked(return_msg, NULL);
    close(sockfd);
}



int handle_incoming_client(int sockfd) {

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int new_client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
    if(new_client_fd == -1) {
		DieWithSystemMessage("accept failed");
    }
    return new_client_fd;
}




void handle_incoming_msg(int sockfd, struct node_info *my_node, Node finger_table[], Node successor_list[],   int num_successors)
{
	ChordMessage *msg = recv_message(sockfd);

	if (msg == NULL) {
		// connection reset
		close(sockfd);
	} else {
		// Init message to return						
		ChordMessage ret_msg = CHORD_MESSAGE__INIT;
		
		switch (msg->msg_case)
		{
		case CHORD_MESSAGE__MSG_FIND_SUCCESSOR_REQUEST: 
		{
			// find succ rec
			ret_msg.msg_case = CHORD_MESSAGE__MSG_FIND_SUCCESSOR_RESPONSE; 

			FindSuccessorResponse succ_resp = FIND_SUCCESSOR_RESPONSE__INIT;
			Node node_to_send = NODE__INIT;
			Node *new_succ = find_successor(my_node->node, my_node->successor, my_node->predecessor, finger_table, msg->find_successor_request->key);
			
			copy_node(&node_to_send, new_succ);
			succ_resp.node = &node_to_send;
			succ_resp.key = msg->find_successor_request->key;

			ret_msg.find_successor_response = &succ_resp;

			send_message(sockfd, &ret_msg);
			free(new_succ);



			break;
		}
		case CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_REQUEST:
		{
			// check pred
			ret_msg.msg_case = CHORD_MESSAGE__MSG_CHECK_PREDECESSOR_RESPONSE;
			CheckPredecessorResponse resp = CHECK_PREDECESSOR_RESPONSE__INIT;
			ret_msg.check_predecessor_response = &resp;
			send_message(sockfd, &ret_msg);
			break;
		}
		case CHORD_MESSAGE__MSG_GET_PREDECESSOR_REQUEST:
		{
			// get pred
			ret_msg.msg_case = CHORD_MESSAGE__MSG_GET_PREDECESSOR_RESPONSE;
			GetPredecessorResponse pred_resp = GET_PREDECESSOR_RESPONSE__INIT;
			Node node_to_send = NODE__INIT;
			pred_resp.node = &node_to_send;
			ret_msg.get_predecessor_response = &pred_resp;

			send_message(sockfd, &ret_msg);

			break;
		}
		case CHORD_MESSAGE__MSG_NOTIFY_REQUEST:
		{

			ret_msg.msg_case = CHORD_MESSAGE__MSG_NOTIFY_RESPONSE;
			NotifyResponse notify_resp = NOTIFY_RESPONSE__INIT;
			ret_msg.notify_response = &notify_resp;

			send_message(sockfd, &ret_msg);

			break;
		}
		case CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_REQUEST:
		{
			
			// Get succ list
			ret_msg.msg_case = CHORD_MESSAGE__MSG_GET_SUCCESSOR_LIST_RESPONSE;

			GetSuccessorListResponse sl_resp = GET_SUCCESSOR_LIST_RESPONSE__INIT;

			sl_resp.n_successors = num_successors;
			
			sl_resp.successors = malloc(sizeof(Node) * num_successors);
			for (int i = 0; i < num_successors; i++) {
				sl_resp.successors[i]->address = successor_list[i].address;
				sl_resp.successors[i]->port = successor_list[i].port;
				sl_resp.successors[i]->key = successor_list[i].key;
			}

			ret_msg.get_successor_list_response = &sl_resp;

			send_message(sockfd, &ret_msg);

			free(sl_resp.successors);
			
			break;
		}
		default:
			break;
		}

		chord_message__free_unpacked(msg, NULL);
	}


}

void lookup(char *str, Node *n, Node *succ, Node *pred, Node ft[]) {
    uint64_t key;
    uint8_t checksum[20];

	hash_value(checksum, str);

    key = sha1sum_truncated_head(checksum);

    Node *node_succ = find_successor(n, succ, pred, ft, key);
    struct in_addr succ_addr;
    succ_addr.s_addr = node_succ->address;

    fprintf(stdout, "< %s ", str);
    fprintf(stdout, "%" PRIu64, key);
    fprintf(stdout, "\n< ");
    fprintf(stdout, "%" PRIu64, node_succ->key);
    fprintf(stdout, " %s %d\n", inet_ntoa(succ_addr), node_succ->port);
}


void print_state(Node *n, Node ft[], Node sl[], int num_succ) {
    // print self
    struct in_addr addr;
    addr.s_addr = n->address;

	fprintf(stdout, "< Self %" PRIu64, n->key);
	fprintf(stdout, " %s %d\n", inet_ntoa(addr), n->port);

    // print succ list
    int i;
    for (i = 0; i < num_succ; i++) {
        addr.s_addr = sl[i].address;
        fprintf(stdout, "< Successor [%d] %" PRIu64, i + 1, sl[i].key);
        fprintf(stdout, " %s %d\n", inet_ntoa(addr), sl[i].port);
    }

    // print finger table
    for (i = 0; i < MAX_FT; i++) {
        addr.s_addr = ft[i].address;
        fprintf(stdout, "< Finger [%d] %" PRIu64, i + 1, ft[i].key);
        fprintf(stdout, " %s %d\n", inet_ntoa(addr), ft[i].port);
    }
}



void handle_inputs(struct node_info *node, Node finger_table[], Node successor_list[],  struct chord_arguments args ) {
    char line[MAX_LINE];
    fgets(line, MAX_LINE, stdin);

	char *command = malloc(sizeof(char) * 11);
	char *param = malloc(sizeof(char) * 1024);

	size_t r = sscanf(line, "%10s %[^\n]s", command, param);
	if(r <= 0){
		show_error_msg("Reading error");
	}else if(r==1){
		if (strcmp("PrintState", command) == 0) {
			print_state(node->node, finger_table, successor_list, args.num_successors);
		} else if (strcmp("quit", command) == 0) {
			exit(0);
		} else {
			// invalid cmd
			printf("Invalid command. Usage: \"Lookup <string>\", \"PrintState\", or \"quit\"\n");
		}
	}else if (strcmp("Lookup", command) == 0) {
		lookup(param, node->node, node->successor, node->predecessor, finger_table);
	}

	
    printf("> ");
    fflush(stdout);
}

void hash_value(uint8_t *result, char *value) {
    struct sha1sum_ctx *ctx = sha1sum_create(NULL, 0);
    sha1sum_finish(ctx, (const uint8_t*)value, strlen(value), result);
    sha1sum_destroy(ctx);
}

 
void printHashHead(uint8_t *hash) {
	uint64_t head = sha1sum_truncated_head(hash);
    printf("%" PRIu64, head);
}

void printKey(uint64_t key)
{
    printf("%" PRIu64, key);
}



int timediff_ds(struct timeval t0, struct timeval t1) //deciseconds
{
	// (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec);
	int diffval = (t1.tv_sec - t0.tv_sec) * 10 + (t1.tv_usec - t0.tv_usec) / 100000.0f;
	return diffval;
}




int main(int argc, char *argv[])
{
    struct chord_arguments args = chord_parseopt(argc, argv);

	if (args.num_successors > 32 || args.num_successors < 1)
    {
		DieWithSystemMessage("successors must be between 1 and 32");
    }


	struct pollfd sockets[MAX_CLIENTS + 2] = {0};

	struct node_info *my_node = malloc(sizeof(struct node_info));
	my_node->node = malloc(sizeof(Node));
	my_node->successor = malloc(sizeof(Node));
	my_node->predecessor = malloc(sizeof(Node));


    // keyboard poll, std input
    sockets[0].fd = STDIN_FILENO;
    sockets[0].events = POLLIN;

	// Listen clients via TCP
	int sockfd = listen_clients(&args.my_address);

	// server poll
    sockets[1].fd = sockfd;
    sockets[1].events = POLLIN | POLLPRI;

	
	Node successor_list[args.num_successors];
	Node finger_table[MAX_FT];
	memset(successor_list, 0, args.num_successors * sizeof(Node));
	memset(finger_table, 0, 64 * sizeof(Node));
	
	init_node(my_node->node, args.my_address);
	if (args.id != 0)
    {
	    my_node->node->key = args.id;
	}


	if (args.join_address.sin_port == 0 && args.join_address.sin_addr.s_addr == 0) {
		// create
		my_node->predecessor = NULL;
		my_node->successor = my_node->node;
		finger_table[0].address = my_node->node->address;
        finger_table[0].port = my_node->node->port;
        finger_table[0].key = my_node->node->key;
	} else {
		// join
		my_node->predecessor = NULL;
		my_node->successor = join(my_node->node, args.join_address);
		finger_table[0].address = my_node->successor->address;
        finger_table[0].port = my_node->successor->port;
        finger_table[0].key = my_node->successor->key;
	}

	// initialize the timevals for the periodic functions
	int sp_t = args.stablize_period * 100; // milliesecond
	int ffp_t = args.fix_fingers_period * 100; // milliesecond
	int cpp_t = args.check_predecessor_period * 100; // milliesecond


    int ready;
    int has_incoming_client = 0;

    printf("> ");
    fflush(stdout);


	struct timeval t_s_prev, t_ff_prev, t_cp_prev;
	gettimeofday(&t_s_prev, NULL);
	gettimeofday(&t_ff_prev, NULL);
	gettimeofday(&t_cp_prev, NULL);

	int next = 0;

    while(1) {		

        ready = poll(sockets, MAX_CLIENTS + 2, 0);
        if(ready < 0) {
			show_error_msg("poll() failed");
        }else if(ready == 0){ // timeout

			struct timeval now;
			gettimeofday(&now, NULL);

			if(timediff_ds(t_s_prev, now) > args.stablize_period){// stabilize
				stabilize(successor_list, args.num_successors, my_node->successor, my_node->node);
				gettimeofday(&t_s_prev, NULL);
			}
			
			if(timediff_ds(t_ff_prev, now) > args.fix_fingers_period){// fix fingers
				fix_fingers(my_node->node, my_node->successor, my_node->predecessor, finger_table, &next);
				gettimeofday(&t_ff_prev, NULL);
			}
			
			if(timediff_ds(t_cp_prev, now) > args.check_predecessor_period){ //check_predecessor
				check_predecessor(my_node->predecessor);
				gettimeofday(&t_cp_prev, NULL);
			}
	

		}else{
			if(sockets[0].revents & POLLIN) {
				handle_inputs(my_node, successor_list,  finger_table, args);
			}

			has_incoming_client = sockets[1].revents & POLLIN;

			for(int i = 0; i < MAX_CLIENTS; i++) {
				if(has_incoming_client) {
					sockets[i+2].fd = handle_incoming_client(sockets[1].fd);
					sockets[i+2].events = POLLIN;
					has_incoming_client = 0;
				} else {
					if(sockets[i+2].revents & POLLIN) {
						//handle incoming message
						int sockfd = sockets[i+2].fd;
						handle_incoming_msg(sockfd, my_node, finger_table, successor_list, args.num_successors);
					}
				}
			}

		}

    }
    close(sockfd);
	free(my_node);

	return 0;

}