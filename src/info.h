#ifndef INFO_H
#define INFO_H

#include <stddef.h>
#include "expr.h"

#define INFO_VAR  1
#define INFO_FUNC 2
#define INFO_COND 3
#define INFO_FOR  4


typedef struct info {
	char *type;
	char *name;
	char _[8192 - 2 * sizeof(char *)];
} info;

typedef struct info_var {
	char *type;
	char *name;
	expr_branch expr;
} info_var;

typedef struct info_func {
	char *type;
	char *name;
	size_t arg_count;
	char *arg_types[256];
	char *arg_names[256];
	char *body;
} info_func;

typedef struct info_for {
	char *var;
	char *action;
	expr_branch expr;
} info_for;

#endif
