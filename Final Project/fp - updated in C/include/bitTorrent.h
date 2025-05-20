#include "bencode.h"
/*This .h file contains functions to be used in bitTorrent.c*/


/*Gets the command keyword from the stdin that the user inputs
*/
void get_command(char *string, char *command);

/*Returns the number of arguments the user passed into the
  terminal. Helps with error checking for specific command
  keywords*/
int num_args(const char sentence[]);

/*Checks wether the provided string from stdin after a "download"
  keyword is a .torrent file. returns 1 on valid input*/
int ends_with_torrent(const char *filename);

/*Takes in an announce URL string and extracts the port number from it.
  The port number of the URL is stored inside "port"*/
void get_port_from_announce_url(const char *announce_url, char *port);

/*Takes in an anounce URL for a tracker and creates a connection between the
  client and the tracker. Also binds the Clients port number to the sockfd.
  returns a sockfd on succesful TCP connection setup.*/
int connect_to_tracker(char *announce_url, char *client_port);

/*Creates the peer id of the client machine on execution*/
char* create_peer_id();

/*Takes in a string and returns the url encoding for that string*/
char* make_url_encode(char* id_str);

/*This function sets up a HTTP GET request using the parameters and sends
  it to the tracker through the sockfd that connects the client and the tracker*/
void send_tracker_request(char*announce_url, char* info_hash, long uploaded, long downloaded, long left, int compact, char* event, int sockfd);

/*Returns the hostname associated with the announce_url from the .torrent file*/
char *extract_tracker_host(const char *announce_url);

