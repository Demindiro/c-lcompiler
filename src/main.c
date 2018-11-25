#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "code/read.h"
#include "code/split.h"
#include "code/branch.h"
#include "code/optimize.h"
#include "asm/gen.h"
#include "util.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Error: no files specified\n");
		return 1;
	}

	debug("\n===   CODE READ   ===\n");
	if (code_read(argv[1]) < 0)
		return 1;

	debug("\n===  CODE  SPLIT  ===\n");
	lines_t *lines = code_split(global_funcs, global_funcs_count);
	if (lines == NULL)
		return 1;
	for (size_t i = 0; i < lines[0].count; i++)
		fprintf(stderr, "%d:%d\t%s\n", lines[0].lines[i].row, lines[0].lines[i].col, lines[0].lines[i].str->buf);
	for (size_t i = 0; i < global_funcs_count; i++) {
		free(global_funcs[i].body);
		global_funcs[i].lines = lines + i;
	}

	debug("\n===  CODE BRANCH  ===\n");
	if (code_branch() < 0)
		return 1;

	debug("\n=== CODE OPTIMIZE ===\n");
	if (code_optimize() < 0)
		return 1;

	debug("\n===    ASM GEN    ===\n");
	if (asm_gen() < 0)
		return 1;

	return 0;
}
