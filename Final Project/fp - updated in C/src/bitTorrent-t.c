#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#define _USE_BSD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <math.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <curl/curl.h>
#include "../include/bitTorrent.h"
// #include "../include/bencode.h"
#include "../include/hash.h"
#include "../include/util.h"

#define MAX_COMMAND_LENGTH 256
#define MAX_RESPONSE_SIZE 4096

char *peer_id;
PeerArr *peer_lst;

int main(int argc, char *argv[])
{

    /*--------------Argument Checking---------------*/
    if (argc != 3 || strcmp(argv[1], "-p") != 0)
    {
        printf("Invalid command line arguments\n");
        exit(0);
    }
    else if (atoi(argv[2]) <= 1024 || atoi(argv[2]) > 65535)
    {
        printf("invalid Port Number\n");
        exit(EXIT_FAILURE);
    }
    /*-----------------------------------------------*/

    curl_global_init(CURL_GLOBAL_ALL);

    char *id_str = create_peer_id();

    peer_id = make_url_encode(id_str);
    peer_lst = intialize_peer_lst();
    // printf("%s\n", peer_id);

    char query[MAX_COMMAND_LENGTH];

    // UNCOMMENT ONCE TORRENT FILE HAS BEEN PARSED

    char *client_port = argv[2];
    // printf("%s\n", client_port);

    struct pollfd fds[1]; // Used to check for activity on stdin
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    int print_sign = 1; // used to manage printing >: (Not Important)

    while (1)
    {

        if (print_sign == 1)
        {
            printf(">: ");
            fflush(stdout);
            print_sign = 0;
        }

        int activity = poll(fds, 1, 0);

        if (activity < 0)
        {
            perror("poll failed");
            exit(EXIT_FAILURE);
        }

        if (fds[0].revents && POLLIN) // if there is activity in stdin
        {

            print_sign = 1;
            fgets(query, MAX_COMMAND_LENGTH, stdin);

            query[strcspn(query, "\n")] = '\0';

            char command[MAX_COMMAND_LENGTH] = {0};
            get_command(query, command);

            if (strcmp(command, "download") == 0 && num_args(query) == 2)
            {
                char *torrent_file = query + strlen("download") + 1;
                torrent_file[strlen(torrent_file)] = '\0';

                if (ends_with_torrent(torrent_file) <= 0)
                {
                    printf("<: Must provide .torrent file\n");
                    continue;
                }

                // download file here
                const char *encoded_str = read_torrent_file(torrent_file);
                if (encoded_str == NULL)
                    continue;

                Tracker *tracker = make_tracker(encoded_str); // contains the parsed information from the .torrent file
                tracker->num_pieces = get_num_pieces(tracker->info);

                char *info_value = bencode_info_value(tracker->info); // bencodes the info dictionary.

                char *announce_url = tracker->announce; // announce url of the racker
                size_t data_len = strlen(info_value);

                unsigned char sha1_hash[SHA_DIGEST_LENGTH + 1]; // buffer to store sha1_hash

                // Compute the SHA-1 hash
                SHA1((unsigned char *)info_value, data_len, sha1_hash);
                sha1_hash[SHA_DIGEST_LENGTH] = '\0';

                char *info_hash = make_url_encode(sha1_hash);

                // Extract the length of the file. Since this is the first request were sending
                long int left = extract_length(tracker->info);
                if (left == -1)
                { // if for whatever reason "length" isn't found in the info dictionary
                    perror("Length Not Found\n");
                    exit(EXIT_FAILURE);
                }

                int sockfd = connect_to_tracker(announce_url, client_port);
                send_tracker_request(announce_url, info_hash, 0, 0, left, 0, "started", sockfd);

                /*This Reading part could be put somewhere else if Needed since this
                  function is meant to just send, but for now it's here
                */
                char buffer[MAX_RESPONSE_SIZE];
                bzero(buffer, MAX_RESPONSE_SIZE);
                int n = read(sockfd, buffer, MAX_RESPONSE_SIZE);
                if (n < 0)
                    perror("ERROR reading from socket");

                // parse tracker response and store in struct
                Tracker_Response *tr_response = parse_tracker_response(buffer);

                //Here we put the peers gotten from the tracker response
                //into our global peer_lst(Type PeerArr*). For now the goal is to make this work
                //for one file so a global peer_lst is fine.
                size_t num_peers = tr_response->peers->size;
                for (size_t i = 0; i < num_peers; i++)
                {
                    DictArr *peer_info = tr_response->peers->items[i]->data.dict;
                    Peer *peer = malloc(sizeof(Peer));

                    for (size_t j = 0; j < peer_info->size; j++)
                    {
                        char *key = peer_info->items[j]->key;

                        if (strcmp(key, "peer id") == 0)
                        {
                            peer->peer_id = malloc(strlen(peer_info->items[j]->value.str));
                            memcpy(peer->peer_id, peer_info->items[j]->value.str, strlen(peer_info->items[j]->value.str));
                        }
                        else if (strcmp(key, "ip") == 0)
                        {
                            peer->ip = malloc(strlen(peer_info->items[j]->value.str));
                            memcpy(peer->ip, peer_info->items[j]->value.str, strlen(peer_info->items[j]->value.str));
                        }
                        else if (strcmp(key, "port") == 0)
                        {
                            peer->port = (int)peer_info->items[j]->value.num;
                        }
                    }

                    add_peer_if_absent(peer_lst, peer);

                    //FROM HERE WE ESTABLISH HANDSHAKES BETWEEN PEERS

                    
                }
                //printf("pop");
            }
            else if (strcmp(query, "list") == 0)
            { // Features for listing peers in swarm
                printf("<: list here []\n");
            }
            else if (strcmp(query, "info") == 0) // Feature for possible info on current client e.g Port number, number of files downloaded and their names, etc.
            {
                printf("<: info here ()()\n");
            }
            else if (strcmp(query, "quit") == 0) // Break out of client
            {
                printf("<: Thank you For Your Time!\n");
                exit(0);
            }
            else
            {
                printf("<: Invalid Command\n");
            }
        }
    }
}

/*---------Dont look after this-------------*/

void get_command(char *string, char *command)
{

    int i = 0;
    for (i = 0; string[i] != '\0' && string[i] != ' '; i++)
    {
        command[i] = string[i];
    }
    command[i] = '\0';
}

int num_args(const char sentence[])
{
    int counted = 0; // result

    // state:
    const char *it = sentence;
    int inword = 0;

    do
        switch (*it)
        {
        case '\0':
        case ' ':
        case '\t':
        case '\n':
        case '\r': // TODO others?
            if (inword)
            {
                inword = 0;
                counted++;
            }
            break;
        default:
            inword = 1;
        }
    while (*it++);

    return counted;
}

int ends_with_torrent(const char *filename)
{
    size_t len = strlen(filename);
    const char *extension = ".torrent";
    size_t ext_len = strlen(extension);

    // If the filename is shorter than the extension, it can't end with the extension
    if (len < ext_len)
    {
        return -1;
    }

    // Compare the end of the filename with the extension
    return strncmp(filename + len - ext_len, extension, ext_len) == 0;
}

void get_port_from_announce_url(const char *announce_url, char *port)
{
    const char *port_start = strrchr(announce_url, ':'); // Find the last colon in the URL
    if (port_start)
    {
        const char *port_end = strchr(port_start, '/'); // Find the first slash after the colon
        if (port_end)
        {
            strncpy(port, port_start + 1, port_end - port_start - 1); // Copy the port number
            port[port_end - port_start - 1] = '\0';                   // Null-terminate the string
        }
    }
}

int connect_to_tracker(char *announce_url, char *client_port)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    char tracker_port_str[5] = {0};
    get_port_from_announce_url(announce_url, tracker_port_str);
    tracker_port_str[4] = '\0';

    char *tracker_host = extract_tracker_host(announce_url);

    // Clear the hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Get address information for the tracker
    if ((rv = getaddrinfo(tracker_host, tracker_port_str, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }

        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof client_addr);
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = INADDR_ANY;
        client_addr.sin_port = htons(atoi(client_port)); // Convert port to network byte order

        if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof client_addr) == -1)
        {
            close(sockfd);
            perror("bind");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            // perror("connect");
            continue;
        }

        break; // Connected successfully
    }

    if (p == NULL)
    {
        fprintf(stderr, "Failed to connect to tracker\n");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int generate_random_number(int min, int max)
{
    return rand() % (max - min + 1) + min;
}

char *create_peer_id()
{
    srand(time(NULL));
    const char *client_id = "FP";
    int version_number = 1924;
    // Allocate memory for the peer ID (including null terminator)
    char *peer_id = (char *)malloc(21 * sizeof(char)); // 20 characters + null terminator
    if (!peer_id)
    {
        perror("Error allocating memory");
        exit(1);
    }

    // Format the peer ID string
    sprintf(peer_id, "-%s%04d-", client_id, version_number);

    // Generate and append random numbers to complete the peer ID
    int remaining_length = 20 - strlen(peer_id); // Length remaining for random numbers
    int len = strlen(peer_id);
    for (int i = 0; i < remaining_length; i++)
    {
        // peer_id[i + strlen(peer_id)] = generate_random_number(0, 9); // Add random digit (0-9)
        sprintf(peer_id + i + len, "%d", generate_random_number(0, 9));
    }
    peer_id[20] = '\0'; // Null-terminate the string

    return peer_id;
}

char *make_url_encode(char *id_str)
{
    CURL *curl = curl_easy_init();
    if (curl)
    {
        char *encoded_id = curl_easy_escape(curl, id_str, 0);

        curl_easy_cleanup(curl);
        return encoded_id;
        // peer_id = encoded_id;
    }
    curl_easy_cleanup(curl);
    exit(EXIT_FAILURE);
}

char *extract_tracker_host(const char *announce_url)
{
    // Find the position of "://" in the announce URL
    const char *protocol_end = strstr(announce_url, "://");
    if (protocol_end == NULL)
    {
        fprintf(stderr, "Invalid announce URL\n");
        return NULL;
    }

    // Move the pointer past "://"
    protocol_end += 3;

    // Find the position of the next '/' after "://"
    const char *path_start = strchr(protocol_end, '/');
    if (path_start == NULL)
    {
        fprintf(stderr, "Invalid announce URL\n");
        return NULL;
    }

    // Calculate the length of the hostname
    if (atoi(path_start - 4) > 0)
    {
        path_start = path_start - 5;
    }
    size_t length = path_start - protocol_end;

    // Allocate memory for the tracker_host string
    char *tracker_host = (char *)malloc((length + 1) * sizeof(char));
    if (tracker_host == NULL)
    {
        perror("Memory allocation error");
        return NULL;
    }

    // Copy the hostname into the tracker_host string
    strncpy(tracker_host, protocol_end, length);
    tracker_host[length] = '\0'; // Null-terminate the string

    return tracker_host;
}

void send_tracker_request(char *announce_url, char *info_hash, long uploaded, long downloaded, long left, int compact, char *event, int sockfd)
{
    // ChatGPT gave me MAX_RESPONSE SIZE so it may be subject to change
    char get_request[MAX_RESPONSE_SIZE];
    int /*portno,*/ n;

    char *tracker_host = extract_tracker_host(announce_url);

    /*char *p = malloc(5);                         // 4 because port in string is stored as 4 bytes
    get_port_from_announce_url(announce_url, p); // HTTP usually runs on port 80
    portno = atoi(p);*/
    // printf(tracker_host);

    // Format the GET request string
    snprintf(get_request, MAX_RESPONSE_SIZE,
             "GET /announce?info_hash=%s&peer_id=%s&port=%d&uploaded=%ld&downloaded=%ld&left=%ld&compact=%d&event=%s HTTP/1.1\r\nHost: %s\r\n\r\n",
             info_hash, peer_id, /*portno*/6880, uploaded, downloaded, left, compact, event, tracker_host);

    // Send the HTTP GET request
    n = write(sockfd, get_request, strlen(get_request));
    if (n < 0)
        perror("ERROR writing to socket");
}
