#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>


// There might be a need to different between a bunch of list or dictionary
typedef enum {
    STR,
    INT,
    LIST, 
    DICT,
    UNDEF
}Type;

struct DictArr;
struct ListArr;

/*This struct is meant to represent a dictionary. This is similar to the List 
 * comment. Please read below to understand 
 */
typedef struct Dict{
    char* key; // The key of a dictionary can only be a string
    union{
        char* str;
        long int num;
        struct DictArr* dict;
        struct ListArr* list;
    }value;   // The value of a dictionary which can be anything
    Type val_type;
}Dict;


// Dynamic, in case we need to resize at runtime
typedef struct DictArr{
    Dict** items;
    size_t size;  // number of elements currently in the dictionary
    // size_t capacity; // curr capacity of the dictionary
} DictArr;


/* This struct is meant to represent a data structure that is capable of 
 * storing elements of different types. Think of a list in python that take
 * heterogeneous elements
 */
typedef struct List{
    union
    {
        char* str;
        long int num;
        struct ListArr* list;
        struct DictArr* dict;
    }data;
    Type type;
} List;

// Dynamic, in case we need to resize at run time
typedef struct ListArr{
    List** items; 
    size_t size;  
    // size_t capacity;
} ListArr;


/* This struct is an amalgamation of a list and a dict. It is intended to be 
 * as a data structure that can take a string, integer, dict or a list. This 
 * will be used in form of an array. i.e An array of AnyList will be used where
 * each index will represent a string, int, dict or list
 * */
typedef struct AnyList{
    union{
        char* str;
        long int num;
        struct DictArr* dict;
        struct ListArr* list;
    }data;
}AnyList;

/* Tracker struct
*/
typedef struct{
    char* announce;
    ListArr* announce_list;
    char* comment;
    char* created_by;
    long int creation_date;
    DictArr* info;
    ListArr* url_list;

    int num_pieces;
} Tracker;


/* Tayo Added Here: Tracker Response struct
   The fields are straight off the website so not all fields may be used
*/
typedef struct{
    char* failure_reason;
    char* warnig_message;//optional
    long int interval; //in seconds. 
    long int min_interval;//optional
    char* tracker_id;//Not entirely sure how this works
    long int complete;//num of peers with entier file
    long int incomplete;//num of non-seeder peers
    ListArr* peers;//List of dictionaries where each dictionary contains information about a peer

}Tracker_Response;

char *parse_str(const char* bencoded_value);
long int parse_int(const char* bencoded_value);
ListArr* parse_as_list(const char* bencoded_value);
DictArr* parse_as_dict(const char* bencoded_value);
Tracker *make_tracker(const char *buffer);

void decode_bencode(const char* bencoded_value);

/*Tayo Added*/

/*Bencodes the values in the info dictionary. May still need work*/
char* bencode_info_value(DictArr* info);

/*Extracts the length of the file from info*/
long int extract_length(DictArr* info);

Tracker_Response* parse_tracker_response(const char *response);

int get_num_pieces(DictArr* info);