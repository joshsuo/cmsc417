#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chord_arg_parser.h"
#include "chord.h"
#include "hash.h"


void printKey(uint64_t key) {
	printf("%" PRIu64, key);
}

int main(int argc, char *argv[]) {
	chord_parseopt(argc, argv);
	return 0;
}
