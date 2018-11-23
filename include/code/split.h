#ifndef SPLIT_H
#define SPLIT_H

#include "include/string.h"
#include "info.h"
#include "read.h"

typedef struct line {
	int row, col;
	string str;
} line_t;

typedef struct lines {
	int count;
	line_t *lines;
} lines_t;

lines_t *code_split(info_func *funcs, size_t count);

#endif
