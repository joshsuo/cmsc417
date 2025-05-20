#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netinet/in.h>
//#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <sys/poll.h>

#define MAX_CLIENTS 256

enum client_state {
    CLIENT_CONNECTING,
    CLIENT_CONNECTED,
    CLIENT_CLOSED
};

enum command {
    CONNECT = 0x9b,
    DISCONNECT = 0xff,
    JOIN = 0x03,
    LEAVE = 0x06,
    KEEPALIVE = 0x13,
    LISTROOM = 0x09,
    LISTUSERS = 0x0c,
    NICK = 0x0f,
    PRIVATEMSG = 0x12,
    CHAT = 0x15
};

enum repsonse {
    RES_CONNECT,
    RES_JOIN,
    RES_JOIN_FAILED,
    RES_LEAVE,
    RES_NICK,
    RES_NICK_FAILED, //err_msg = Command failed. (You can't take someone else's nick, or their code either.)
    RES_MSG,
    RES_LIST_ROOMS,
    RES_LIST_USERS,
    RES_CHAT,
    RES_CHAT_FAILED
};

static const int MAXPENDING = 5; // Maximum outstanding connection requests

struct room_info {
    char *name;
    char *pwd;
    int user_count;
    struct room_info *next;
};

struct client_info {
    enum client_state state;
    int fd;
    char nick[256];
    struct room_info *room;
};

struct room_info *room_list = NULL;

struct server_arguments {
	int port;
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
	// case 's':
	// 	args->salt_len = strlen(arg);
	// 	args->salt = malloc(args->salt_len+1);
	// 	strcpy(args->salt, arg);
	// 	break;
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
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, args) != 0) {
		fprintf(stderr, "Got an error condition when parsing\n");
	}

    // if (!args->port) {
	// 	fputs("A port number must be specified\n", stderr);
	// 	exit(1);
	// }

	/* What happens if you don't pass in parameters? Check args values
	 * for sanity and required parameters being filled in */

	/* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
	//printf("Got port %d\n", args->port);
	//free(args.salt);

}

void dieWithMsg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

int read_bytes(int sockfd, void *buffer, int bytes) {
    int bytes_recieved = 0;
    int temp = 0;

    while(bytes_recieved < bytes) {
        temp = recv(sockfd, buffer + bytes_recieved, bytes-bytes_recieved, 0);
        if(temp == -1) {
            fprintf(stderr, "read failed\n");
            return -1;
        } else if(temp == 0) {
            return 0;
        }
        bytes_recieved += temp;
    }
    return bytes_recieved;
}

void send_bytes(int sockfd, void *buffer, int bytes) {
    int bytes_sent = 0;
    int temp = 0;

    while(bytes_sent < bytes) {
        temp = send(sockfd, buffer + bytes_sent, bytes - bytes_sent, 0);
        if(temp == -1) {
            fprintf(stderr, "send failed\n");
            break;
        }
        bytes_sent += temp;
    }
}

void send_error(struct client_info *to_client, char *message) {
    int msg_len = strlen(message);
    uint32_t content_len = msg_len + 1;
    int buff_size = 8 + msg_len;
    uint16_t magic_num = 0x0417;
    uint16_t flag = 0x9a01;

    uint8_t *buffer = malloc(buff_size);
    memset(buffer, 0, buff_size);

    content_len = htonl(content_len);
    memcpy(buffer, &content_len, 4);
    magic_num = htons(magic_num);
    memcpy(buffer + 4, &magic_num, 2);
    flag = htons(flag);
    memcpy(buffer + 6, &flag, 2);
    memcpy(buffer + 8, message, msg_len);

    send_bytes(to_client->fd, buffer, buff_size);
    free(buffer);
}

struct room_info *get_room_by_name(char *name) {
    struct room_info *current = room_list;
    while(current != NULL) {
        if(!strcmp(current->name, name)) {
            return current;
        }
        current = current->next;
    }
    return 0x0;
}

struct client_info *get_client_by_nick(struct client_info *clients, char *nick) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!strcmp(clients[i].nick, nick)) {
            return &clients[i];
        }
    }
    return 0x0;
}

void send_message(struct client_info *from_client, struct client_info *to_client, char *message) {
    uint8_t from_nick_len = strlen(from_client->nick);
    int msg_size = strlen(message);
    uint16_t msg_len = msg_size;

    uint32_t content_len = 1 + from_nick_len + 2 + msg_len;
    uint8_t msg_buff_size = 7 + content_len;

    uint8_t *msg_buffer = malloc(msg_buff_size + 1);
    memset(msg_buffer, 0, msg_buff_size + 1);

    content_len = htonl(content_len);
    memcpy(msg_buffer, &content_len, 4);
    uint16_t magic_num = 0x0417;
    magic_num = htons(magic_num);
    memcpy(msg_buffer + 4, &magic_num, 2);
    uint8_t flag = 0x12;
    memcpy(msg_buffer + 6, &flag, 1);
    memcpy(msg_buffer + 7, &from_nick_len, 1);
    memcpy(msg_buffer + 8, from_client->nick, from_nick_len);
    msg_len = htons(msg_len);
    memcpy(msg_buffer + 8 + from_nick_len, &msg_len, 2);
    memcpy(msg_buffer + 8 + from_nick_len + 2, message, msg_size);

    send_bytes(to_client->fd, msg_buffer, msg_buff_size);
    free(msg_buffer);
}

void send_message_to_room(struct client_info *from_client, struct room_info *room, struct client_info *clients, char *message) {
    uint8_t room_len = strlen(room->name);
    uint8_t from_nick_len = strlen(from_client->nick);
    int msg_size = strlen(message);
    uint16_t msg_len = msg_size;

    uint32_t content_len = 1 + room_len + 1 + from_nick_len + 2 + msg_len;
    uint8_t msg_buff_size = 7 + content_len;

    uint8_t *msg_buffer = malloc(msg_buff_size + 1);
    memset(msg_buffer, 0, msg_buff_size + 1);

    content_len = htonl(content_len);
    memcpy(msg_buffer, &content_len, 4);
    uint16_t magic_num = 0x0417;
    magic_num = htons(magic_num);
    memcpy(msg_buffer + 4, &magic_num, 2);
    uint8_t flag = 0x15;
    memcpy(msg_buffer + 6, &flag, 1);
    memcpy(msg_buffer + 7, &room_len, 1);
    memcpy(msg_buffer + 8, room->name, room_len);
    memcpy(msg_buffer + 8 + room_len, &from_nick_len, 1);
    memcpy(msg_buffer + 8 + room_len + 1, from_client->nick, from_nick_len);
    msg_len = htons(msg_len);
    memcpy(msg_buffer + 8 + room_len + 1 + from_nick_len, &msg_len, 2);
    memcpy(msg_buffer + 8 + room_len + 1 + from_nick_len + 2, message, msg_size);

    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].room == room && clients[i].fd != from_client->fd) {
            send_bytes(clients[i].fd, msg_buffer, msg_buff_size);
        }
    }

    free(msg_buffer);
}

void send_response_to_client(int client_fd, struct client_info *clients, int client_index, int response) {
    int buff_size;
    int list_len = 0;
    int pos = 0;
    int msg_len = 0;
    uint32_t content_len = 0;
    uint16_t magic_num = 0x0417;
    uint16_t flag = 0x9a00;
    uint8_t *buffer = NULL;
    struct client_info *client = (clients + client_index);

    switch(response) {
        case RES_CONNECT:
            sprintf(client->nick, "rand%d", client_index);
            int name_size = strlen(client->nick);
            buff_size = 4 + 4 + name_size;
            uint32_t name_len = name_size + 1;

            uint8_t *buffer3 = malloc(buff_size);
            memset(buffer3, 0, buff_size);

            name_len = htonl(name_len);
            memcpy(buffer3, &name_len, 4);
            magic_num = htons(magic_num);
            memcpy(buffer3 + 4, &magic_num, 2);
            flag = htons(flag);
            memcpy(buffer3 + 6, &flag, 2);
            memcpy(buffer3 + 8, client->nick, name_size);

            send_bytes(client_fd, buffer3, buff_size);
            free(buffer3);
            client->state = CLIENT_CONNECTED;
        break;

        case RES_JOIN:
        case RES_LEAVE:
        case RES_NICK:
        case RES_CHAT:
        case RES_MSG:
            buff_size = 8;
            buffer = malloc(buff_size);
            memset(buffer, 0, buff_size);
            content_len = htonl(1);
            memcpy(buffer, &content_len, 4);
            magic_num = htons(magic_num);
            memcpy(buffer + 4, &magic_num, 2);
            flag = htons(flag);
            memcpy(buffer + 6, &flag, 2);
            send_bytes(client_fd, buffer, buff_size);
            free(buffer);
        break;

        case RES_JOIN_FAILED:
            send_error(client, "Invalid password. Try deleting the 1 at the end.");
            // const char *err_msg = "Invalid password. Try deleting the 1 at the end.";
            // msg_len = strlen(err_msg);
            // content_len = msg_len + 1;
            // buff_size = 8 + strlen(err_msg);
            // flag = 0x9a01;

            // buffer = malloc(buff_size);
            // memset(buffer, 0, buff_size);

            // content_len = htonl(content_len);
            // memcpy(buffer, &content_len, 4);
            // magic_num = htons(magic_num);
            // memcpy(buffer + 4, &magic_num, 2);
            // flag = htons(flag);
            // memcpy(buffer + 6, &flag, 2);
            // memcpy(buffer + 8, err_msg, msg_len);

            // send_bytes(client_fd, buffer, buff_size);
            // free(buffer);

        break;

        case RES_LIST_ROOMS:
            struct room_info *current = room_list;
            list_len = 0;
            while(current != NULL) {
                list_len += 1 + strlen(current->name);
                current = current->next;
            }

            buff_size = 8 + list_len;
            buffer = malloc(buff_size);
            memset(buffer, 0, buff_size);

            content_len = htonl(list_len + 1);
            memcpy(buffer, &content_len, 4);
            magic_num = htons(magic_num);
            memcpy(buffer + 4, &magic_num, 2);
            flag = htons(flag);
            memcpy(buffer + 6, &flag, 2);

            current = room_list;
            pos = 8;
            while(current != NULL) {
                uint8_t name_len = strlen(current->name);
                memcpy(buffer + pos, &name_len, 1);
                memcpy(buffer + pos + 1, current->name, name_len);
                pos += 1 + name_len;
                current = current->next;
            }
            send_bytes(client_fd, buffer, buff_size);
            free(buffer);
        break;

        case RES_LIST_USERS:
            list_len = 0;
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(client->room != NULL && clients[i].room == client->room
                    || client->room == NULL && clients[i].state == CLIENT_CONNECTED) {
                        list_len += 1 + strlen(clients[i].nick);
                    }
            }

            buff_size = 8 + list_len;

            buffer = malloc(buff_size);
            memset(buffer, 0, buff_size);

            content_len = htonl(list_len + 1);
            memcpy(buffer, &content_len, 4);
            magic_num = htons(magic_num);
            memcpy(buffer + 4, &magic_num, 2);
            flag = htons(flag);
            memcpy(buffer + 6, &flag, 2);

            pos = 8;
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(client->room != NULL && clients[i].room == client->room
                    || client->room == NULL && clients[i].state == CLIENT_CONNECTED) {
                        uint8_t name_len = strlen(clients[i].nick);
                        memcpy(buffer + pos, &name_len, 1);
                        memcpy(buffer + pos + 1, clients[i].nick, name_len);
                        pos += 1 + name_len;
                    }
            }
            send_bytes(client_fd, buffer, buff_size);
            free(buffer);
        break;

        case RES_CHAT_FAILED:
            send_error(client, "You speak to no one. There is no one here.");
        break;

        case RES_NICK_FAILED:
            send_error(client, "You can't take someone else's nick, or their code either.");
        break;

    }

}

int handle_incoming_client(int servSock, struct client_info *client) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int new_client_fd = accept(servSock, (struct sockaddr *)&client_addr, &addr_len);
    if(new_client_fd == -1) {
        dieWithMsg("accept failed");
    }

    memset(client, 0, sizeof(*client));

    client->fd = new_client_fd;
    client->state = CLIENT_CONNECTING;

    return new_client_fd;

}

void handle_incoming_msg(struct client_info *clients, struct pollfd *sockets, int index) {
    struct client_info *client = (clients + index);

    int client_fd = client->fd;
    int read_count = 0;

    // uint8_t *buffer = malloc(1024);
    // read_count = recv(client_fd, buffer, 1024, 0);

    // for(int i = 0; i < read_count; i++) {
    //     printf("%02x", *(buffer+i));
    // }
    // printf("\n");

    // free(buffer);

    uint8_t command = 0;

    uint32_t content_size = 0;
    read_count = read_bytes(client_fd, &content_size, 4);
    if(read_count == 0) {
        command = DISCONNECT;
    } else {
        content_size = ntohl(content_size);
        uint16_t magic_num = 0;
        read_count = read_bytes(client_fd, &magic_num, 2);
        if(read_count == 0) {
            command = DISCONNECT;
        } else {
            magic_num = ntohs(magic_num);
            read_count = read_bytes(client_fd, &command, 1);
            if(read_count == 0) {
                command = DISCONNECT;
            } else {
                //printf("content_size = 0x%02x, magic_num = 0x%02x, command = 0x%02x\n", content_size, magic_num, command);
            }
        }
    }

    if(command == CONNECT) {
        uint8_t content[content_size];
        read_bytes(client_fd, content, content_size);

        send_response_to_client(client_fd, clients, index, RES_CONNECT);

        //for(int i = 0; i < content_size; i++) printf("%02x", (content+i));
        //printf("\n");

    } else if(command == KEEPALIVE) {
        uint8_t content[content_size];
        read_bytes(client_fd, content, content_size);

    }else if(command == DISCONNECT) {
        fprintf(stderr, "disconnected: sockfd = %d, nick = %s\n", client->fd, client->nick);
        close(client->fd);
        client->fd = -1;
        sockets[index+1].fd = -1;
        memset(client->nick, 0, sizeof(client->nick));
        client->room = NULL;
        client->state = CLIENT_CLOSED;

    } else if(command == JOIN) {
        uint8_t room_name_len = 0;
        read_bytes(client_fd, &room_name_len, 1);
        uint8_t room_name[room_name_len];
        read_bytes(client_fd, room_name, room_name_len);
        room_name[room_name_len] = 0;
        uint8_t pwd_len = 0;
        read_bytes(client_fd, &pwd_len, 1);

        uint8_t *pwd = 0;
        if(pwd_len > 0) {
            pwd = malloc(pwd_len + 1);
            memset(pwd, 0, pwd_len);
            read_bytes(client_fd, pwd, pwd_len);
            pwd[pwd_len] = 0;
        }
        

        //printf("pwd=%s, room_name=%s", pwd, room_name);

        struct room_info *room = get_room_by_name(room_name);
        if(room == NULL) {
            room = malloc(sizeof(struct room_info));
            memset(room, 0, sizeof(struct room_info));
            room->name = malloc(room_name_len +1);
            room->pwd = malloc(pwd_len + 1);
            strcpy(room->name, room_name);
            room->name[room_name_len] = 0;

            if(pwd_len > 0) {
                strcpy(room->pwd, pwd);
                room->pwd[pwd_len] = 0;
            } else {
                room->pwd = NULL;
            }
            
            room->user_count = 1;

            room->next = room_list;
            room_list = room;

            client->room = room;

            send_response_to_client(client_fd, clients, index, RES_JOIN);
        } else {
            if((room->pwd != NULL && pwd != NULL && !strcmp(room->pwd, pwd))
                || (room->pwd == NULL && pwd == NULL)) {
                room->user_count++;
                client->room = room;

                send_response_to_client(client_fd, clients, index, RES_JOIN);
            } else {
                fprintf(stderr, "pwd is wrong");
                send_response_to_client(client_fd, clients, index, RES_JOIN_FAILED);
            }
        }
    } else if(command == LEAVE) {
        if(client->room != NULL) {
            struct room_info *room = client->room;
            room->user_count--;
            if(room->user_count == 0) {
                struct room_info *current = room_list;
                struct room_info *prev = NULL;
                while(current != NULL) {
                    if(current == room) {
                        if(prev != NULL) {
                            prev->next = current->next;
                        }
                        if(current == room_list) {
                            room_list = current->next;
                        }
                        free(current->name);
                        free(current->pwd);
                        free(current);
                        break;
                    }
                    prev = current;
                    current = current->next;
                }
            }
            client->room = NULL;
            send_response_to_client(client_fd, clients, index, RES_LEAVE);
        } else {
            close(client->fd);
            client->fd = -1;
            sockets[index + 1].fd = -1;
            memset(client->nick, 0, sizeof(client->nick));
            client->room = NULL;
            client->state = CLIENT_CLOSED;
        }
    }else if(command == LISTROOM) {
        send_response_to_client(client_fd, clients, index, RES_LIST_ROOMS);
    } else if(command == LISTUSERS) {
        send_response_to_client(client_fd, clients, index, RES_LIST_USERS);
    } else if(command == NICK) {
        uint8_t nick_len = 0;
        read_bytes(client_fd, &nick_len, 1);
        uint8_t nick_name[nick_len];
        read_bytes(client_fd, nick_name, nick_len);
        nick_name[nick_len] = 0;

        struct client_info *another_client = get_client_by_nick(clients, nick_name);
        if(another_client != NULL && another_client != client) {
            send_response_to_client(client_fd, clients, index, RES_NICK_FAILED);
        } else {
            sprintf(client->nick, "%s", nick_name);
            send_response_to_client(client_fd, clients, index, RES_NICK);
        }
       
    } else if (command == PRIVATEMSG) {
        uint8_t nick_size_len = 0;
        read_bytes(client_fd, &nick_size_len, 1);

        uint8_t nick_name[nick_size_len];
        read_bytes(client_fd, nick_name, nick_size_len);
        nick_name[nick_size_len] = 0;

        uint16_t msg_len = 0;
        read_bytes(client_fd, &msg_len, 2);
        msg_len = ntohs(msg_len);

        uint8_t msg[msg_len];
        read_bytes(client_fd, msg, msg_len);
        msg[msg_len] = 0;

        struct client_info *to_client = get_client_by_nick(clients, nick_name);

        send_message(client, to_client, msg);

        send_response_to_client(client_fd, clients, index, RES_MSG);
    } else if(command == CHAT) {
        uint8_t room_len = 0;
        read_bytes(client_fd, &room_len, 1);

        if(room_len == 0) {
            send_response_to_client(client_fd, clients, index, RES_CHAT_FAILED);
        } else {
            uint8_t room_name[room_len];
            memset(room_name, 0, room_len);
            read_bytes(client_fd, room_name, room_len);
            room_name[room_len] = 0;

            struct room_info *room = get_room_by_name(room_name);

            uint16_t msg_len = 0;
            read_bytes(client_fd, &msg_len, 2);

            msg_len = ntohs(msg_len);
            uint8_t msg[msg_len];
            memset(msg, 0, msg_len);
            read_bytes(client_fd, msg, msg_len);
            msg[msg_len] = 0;

            send_message_to_room(client, room, clients, msg);
            send_response_to_client(client_fd, clients, index, RES_CHAT);
        }
    }


}



int main(int argc, char *argv[]) {

    struct server_arguments args;
    server_parseopt(&args, argc, argv);

    struct client_info client_list[MAX_CLIENTS] = {0}; // all client info
    struct pollfd sockets[MAX_CLIENTS + 1] = {0}; // include server socket

    // Create socket for incoming connections
    int servSock; // Socket descriptor for server
    if ((servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        dieWithMsg("socket() failed");    
    }

    sockets[0].fd = servSock;
    sockets[0].events = POLLIN;

    // set initial values -1
    for(int i = 0; i < MAX_CLIENTS; i++) {
        sockets[i + 1].fd = -1;
    }

    // Construct local address structure
    struct sockaddr_in servAddr; // Local address
    memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
    servAddr.sin_family = AF_INET; // IPv4 address family
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
    servAddr.sin_port = htons(args.port); // Local port

    // Bind to the local address
    if (bind(servSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0) {
        dieWithMsg("bind() failed");
    }

    // Mark the socket so it will listen for incoming connections
    if (listen(servSock, MAXPENDING) < 0) {
        dieWithMsg("listen() failed");
    }

    int ready;
    int has_incoming_client = 0;

    while(1) {
        ready = poll(sockets, MAX_CLIENTS + 1, -1); // -1 means wait for activity
        if(ready < 0) {
            dieWithMsg("poll() failed");
        }

        has_incoming_client = sockets[0].revents & POLLIN;

        for(int i = 0; i < MAX_CLIENTS; i++) {
            if(sockets[i+1].fd < 0) {
                if(has_incoming_client) {
                    sockets[i+1].fd = handle_incoming_client(servSock, &client_list[i]);
                    sockets[i+1].events = POLLIN;
                    has_incoming_client = 0;
                }
            } else {
                if(sockets[i+1].revents & POLLIN) {
                    handle_incoming_msg(client_list, sockets, i);
                }
            }
        }

        // struct sockaddr_in clntAddr; // Client address
        // // Set length of client address structure (in-out parameter)
        // socklen_t clntAddrLen = sizeof(clntAddr);

        // // Wait for a client to connect
        // int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
        // if (clntSock < 0) {
        //    dieWithMsg("accept() failed");
        // }

        // // clntSock is connected to a client!
        // char clntName[INET_ADDRSTRLEN]; // String to contain client address
        // if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName, sizeof(clntName)) != NULL) {
        //     printf("Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
        // }
        // else {
        //     puts("Unable to get client address");
        // }

    }

    close(servSock);
    
    //printf("port: %d\n", args.port);

    //printf("test\n");

    return 0;
}