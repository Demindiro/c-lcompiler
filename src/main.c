#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "code/read.h"
#include "code/branch.h"
#include "asm/gen.h"
#include "util.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Error: no files specified\n");
		return 1;
	}
	debug("\n===  CODE READ  ===\n");
	if (code_read(argv[1]) < 0)
		return 1;
	debug("\n=== CODE BRANCH ===\n");
	if (code_branch() < 0)
		return 1;
	debug("\n===   ASM GEN   ===\n");
	if (asm_gen() < 0)
		return 1;
}
