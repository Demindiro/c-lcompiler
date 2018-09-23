#include <stdio.h>
#include <string.h>
#include "../table.h"
#include "../code-read/main.h"

const char *keywords[] = {
	"sbyte",
	"sshort",
	"sint",
	"slong",
	"ubyte",
	"ushort",
	"uint",
	"ulong",
	"void",
};

static int is_l_keyword(char *str)
{
	for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); i++)
		if (strcmp(str, keywords[i]) == 0)
			return 1;
	return 0;
}

static int parse(char **pptr, char *end, info *inf, table *tbl)
{
	char *ptr = strtok(*pptr, "\t \n");
	if (is_l_keyword(ptr)) {
		code_parse(&ptr, end, inf);
	} else {
		char *val;
		if (table_get(tbl, ptr, val) < 0) {
			printf("Entry undefined: %s\n", ptr);
			return -1;
		}
	}
	*pptr = ptr;
}

static int convert_var(info_var *inf)
{
	printf("%s:\n", inf->name);
	if (strcmp(inf->type, "string") == 0) {
		printf(".string\t\"%s\"\n", inf->value);
	} else if (strcmp(inf->type, "sbyte" ) == 0) {
		printf(".byte\t%s\n", inf->value);
	} else if (strcmp(inf->type, "sshort") == 0) {
		printf(".half\t%s\n", inf->value);
	} else if (strcmp(inf->type, "sint"  ) == 0) {
		printf(".word\t%s\n", inf->value);
	} else if (strcmp(inf->type, "slong" ) == 0) {
		printf(".dword\t%s\n", inf->value);
	} else {
		printf("Unknown type: %s\n", inf->type);
		return -1;
	}
	printf("\n");
}

static int convert_func(info_func *inf)
{
	table tbl;
	table_init(&tbl, sizeof(char *), sizeof(char *));
	char *saveptr;
	printf("%s:\n", inf->name);
	char *ptr = inf->body;
	char *end = ptr + strlen(inf->body);
	while (1) {
		info i;
		switch (parse(&ptr, end, &i, &tbl)) {
		case 0:
			goto double_break;
		case INFO_VAR:
			break;
		case INFO_FUNC:
			convert_func((info_func *)&i);
			break;
		}
		if (ptr == NULL)
			break;
	}
double_break:
	return 0;
}

int asm_gen()
{
	printf("DATA\n");
	for (size_t i = 0; i < global_vars_count; i++) {
		convert_var(&global_vars[i]);	
	}
	printf("TEXT\n");
	for (size_t i = 0; i < global_funcs_count; i++) {
		convert_func(&global_funcs[i]);
	}
}
