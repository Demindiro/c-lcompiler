#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "code-read/main.h"
#include "asm-gen/main.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Error: no files specified\n");
		return 1;
	}
	printf("\n=== CODE READ ===\n");
	code_read(argv[1]);
	printf("\n===  ASM GEN  ===\n");
	asm_gen();
}
