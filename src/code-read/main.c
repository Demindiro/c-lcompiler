#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "../info.h"

#define WHITE(x) (x == ' ' || x == '\t')


static int parse_var(char **pptr, info *inf)
{
	info_var *iv = (info_var *)inf;
	char *ptr = *pptr;
	iv->value = ptr;
	while (!WHITE(*ptr) && *ptr != ';')
		ptr++;
	char c = *ptr;
	*(ptr++) = 0;
	if (c != ';') {
		while (*ptr != ';')
			ptr++;
		ptr++;
	}
	*pptr = ptr;
	return 0;
}

static int parse_func(char **pptr, info *inf)
{
	// no 'if' because, well...
	info_func *iv = (info_func *)inf;
	char *ptr = *pptr;
	size_t i;
	for (i = 0; i < sizeof(iv->arg_types); i++) {
		while (WHITE(*ptr))
			ptr++;
		iv->arg_types[i] = ptr;
		while (!WHITE(*ptr))
			ptr++;
		*(ptr++) = 0;
		while (WHITE(*ptr))
			ptr++;
		iv->arg_names[i] = ptr;
		while (!WHITE(*ptr) && *ptr != ',' && *ptr != ')')
			ptr++;
		char c = *ptr;
		*(ptr++) = 0;
		if (c == ')')
			goto done;
		if (c == ',')
			continue;
		while (*ptr != ',') {
			if (*ptr == ')') {
				*(ptr++) = 0;
				goto done;
			}
		}
	}
done:
	iv->arg_count = i + 1;
	while (*ptr != '{')
		ptr++;
	iv->body = ++ptr;
	while (*ptr != '}')
		ptr++;
	*(ptr++) = 0;
	*pptr = ptr;
	return 0;
}


int code_parse(char **pptr, char *end, info *inf)
{
	char *ptr = *pptr;
	while (WHITE(*ptr) || *ptr == '\n') {
		ptr++;
		if (ptr >= end)
			return 0;
	}
	inf->type = ptr;
	while (!WHITE(*ptr))
		ptr++;
	*(ptr++) = 0;
	inf->name = ptr;
	while (!WHITE(*ptr) && *ptr != '(' && *ptr != '=')
		ptr++;
	if (WHITE(*ptr)) {
		*(ptr++) = 0;
		while (WHITE(*ptr))
			ptr++;
	}
	char c = *ptr;
	*(ptr++) = 0;
	while (WHITE(*ptr))
		ptr++;
	int t;
	if (c == '=') {
		parse_var (&ptr, inf);
		t = INFO_VAR;
	} else {
		parse_func(&ptr, inf);
		t = INFO_FUNC;
	}
	*pptr = ptr;
	return t;
}


int code_read(char *file)
{
	char buf[1024*1024];
	int fd = open(file, O_RDONLY);
	size_t len = read(fd, buf, sizeof(buf));
	char *ptr = buf;
	while (1) {
		info inf;
		switch (code_parse(&ptr, buf + len, &inf)) {
		case 0:
			goto done;
		case INFO_VAR:
			if (global_vars_size == global_vars_count)
				global_vars = realloc(global_vars, (global_vars_size + 10) * 3 / 2 * sizeof(info_var));
			memcpy(&global_vars[global_vars_count++], &inf, sizeof(info_var));
			break;
		case INFO_FUNC:
			if (global_funcs_size == global_funcs_count)
				global_funcs = realloc(global_funcs, (global_funcs_size + 10) * 3 / 2 * sizeof(info_func));
			memcpy(&global_funcs[global_funcs_count++], &inf, sizeof(info_func));
			break;
		}
	}
done:
	printf("GLOBAL VARIABLES: %lu\n", global_vars_count);
	for (size_t i = 0; i < global_vars_count; i++) {
		info_var *iv = &global_vars[i];
		printf("  name: '%s',  type: '%s',  value: '%s'\n", iv->name, iv->type, iv->value);
	}
	printf("GLOBAL FUNCTIONS: %lu\n", global_funcs_count);
	for (size_t i = 0; i < global_funcs_count; i++) {
		info_func *iv = &global_funcs[i];
		printf("  name: '%s', type: '%s'\n", iv->name, iv->type);
		for (size_t i = 0; i < iv->arg_count; i++)
			printf("    name: '%s', type: '%s'\n", iv->arg_names[i], iv->arg_types[i]);
		printf("'''%s'''\n", iv->body);
	}
	return 0;
}
