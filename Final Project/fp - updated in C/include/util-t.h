#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>


typedef struct Peer{
    char* ip;
    int port;
    char* peer_id;
}Peer;

typedef struct PeerArr{
    Peer** items;
    size_t size;
}PeerArr;

bool is_digit(char c);
// char *read_torrent_file(const char *filename, unsigned char **sha1_buffer,size_t *hash_len);
char *read_torrent_file(const char *filename);

int countDigits(int number);
void list_iterate(const char **bencoded_value);
void dict_iterate(const char **bencoded_value);

// printing
void printList(ListArr *list);
void printDict(DictArr *dict);
void printTracker(Tracker *track);
void printData(void *list, Type type);

//bencodiing
char *bencode(DictArr *info);
char *bencode_helper(char *key, char *key_bencode, int buf_size, DictArr *info, int i);

void print_sha1_hex(unsigned char *hash);

PeerArr* intialize_peer_lst();

void add_peer_if_absent(PeerArr* peer_lst, Peer* peer);