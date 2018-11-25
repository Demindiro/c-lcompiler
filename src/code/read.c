#include "code/read.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "info.h"
#include "util.h"
#include "expr.h"


static int parse_var(char **pptr, info *inf)
{
	info_var *iv = (info_var *)inf;
	char *ptr = *pptr, *orgptr = ptr;
	SKIP_WHITE(ptr);
	fprintf(stderr, "TODO (parse_var)\n");
	return -1;
	while (!IS_WHITE(*ptr) && *ptr != ';')
		ptr++;
	char c = *ptr;
	*(ptr++) = 0;
	if (c != ';' && c != '\n') {
		while (*ptr != ';' && *ptr != '\n')
			ptr++;
		ptr++;
	}
	*pptr = ptr;
	return 0;
}

static int parse_func(char **pptr, info *inf)
{
	info_func *iv = (info_func *)inf;
	char *ptr = *pptr, *orgptr;
	size_t i, len;
	for (i = 0; i < sizeof(iv->arg_types) / sizeof(*iv->arg_types); i++) {
		SKIP_WHITE(ptr);
		orgptr = ptr;
		while (!IS_WHITE(*ptr) && *ptr != ')')
			ptr++;
		if (*ptr == ')') {
			i--;
			ptr++;
			break;
		}
		len = ptr - orgptr;
		iv->arg_types[i] = malloc(len + 1);
		memcpy(iv->arg_types[i], orgptr, len);
		iv->arg_types[i][len] = 0;

		SKIP_WHITE(ptr);

		orgptr = ptr;
		while (!IS_WHITE(*ptr) && *ptr != ',' && *ptr != ')')
			ptr++;
		len = ptr - orgptr;
		iv->arg_names[i] = malloc(len + 1);
		memcpy(iv->arg_names[i], orgptr, len);
		iv->arg_names[i][len] = 0;
		
		SKIP_WHITE(ptr);

		if (*ptr == ')')
			break;
		if (*(ptr++) == ',')
			continue;
	}
	iv->arg_count = i + 1;
	while (*ptr != '{')
		ptr++;
	ptr++;
	orgptr = ptr;
	int c = 0;
	while (1) {
		if (*ptr == '}')
			c--;
		else if (*ptr == '{')
			c++;
		if (c < 0)
			break;
		ptr++;
	}
	len = ptr - orgptr;
	iv->body = malloc(len + 1);
	memcpy(iv->body, orgptr, len);
	iv->body[len] = 0;
	*pptr = ptr + 1;
	return 0;
}


int code_parse(char **pptr, char *end, info *inf)
{
	char *ptr = *pptr, *orgptr;
	size_t len;
	
	SKIP_WHITE(ptr);
	
	if (*ptr == 0)
		return 0;

	orgptr = ptr;
	while (!IS_WHITE(*ptr))
		ptr++;
	len = ptr - orgptr;
	inf->type = string_create(orgptr, len);
	
	SKIP_WHITE(ptr);
	
	orgptr = ptr;
	while (!IS_WHITE(*ptr) && *ptr != '(' && *ptr != '=')
		ptr++;
	len = ptr - orgptr,
	inf->name = string_create(orgptr, len);
	
	SKIP_WHITE(ptr);
	
	char c = *(ptr++);
	int t;
	if (c == '=') {
		parse_var (&ptr, inf);
		t = 1;
	} else {
		parse_func(&ptr, inf);
		t = 2;
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
		case 1:
			if (global_vars_size == global_vars_count)
				global_vars = realloc(global_vars, (global_vars_size + 10) * 3 / 2 * sizeof(info_var));
			memcpy(&global_vars[global_vars_count++], &inf, sizeof(info_var));
			break;
		case 2:
			if (global_funcs_size == global_funcs_count)
				global_funcs = realloc(global_funcs, (global_funcs_size + 10) * 3 / 2 * sizeof(info_func));
			memcpy(&global_funcs[global_funcs_count++], &inf, sizeof(info_func));
			break;
		}
	}
done:
#if 0 || 1
	debug("GLOBAL VARIABLES: %lu", global_vars_count);
	for (size_t i = 0; i < global_vars_count; i++) {
		info_var *iv = &global_vars[i];
		debug("  name: '%s',  type: '%s'", iv->name->buf, iv->type->buf); // TODO
	}
	debug("GLOBAL FUNCTIONS: %lu", global_funcs_count);
	for (size_t i = 0; i < global_funcs_count; i++) {
		info_func *iv = &global_funcs[i];
		debug("  name: '%s', type: '%s'", iv->name->buf, iv->type->buf);
		for (size_t i = 0; i < iv->arg_count; i++)
			debug("    name: '%s', type: '%s'", iv->arg_names[i], iv->arg_types[i]);
		debug("'''%s'''", iv->body);
	}
#endif
	return 0;
}
