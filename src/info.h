#ifndef INFO_H
#define INFO_H

#include <stddef.h>

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
	char *value;
} info_var;

typedef struct info_func {
	char *type;
	char *name;
	size_t arg_count;
	char *arg_types[256];
	char *arg_names[256];
	char *body;
} info_func;

/*
typedef struct info_cond {
	char *type;
	char *name;
	char *cond;
} info_cond;
*/

typedef struct info_for {
	//char *type;
	//char *name;
	//char *cond;
	char *it_var;
	char *it_expr;
	char *it_action;
} info_for;

#endif
