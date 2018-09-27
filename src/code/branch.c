#include "branch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "read.h"
#include "../util.h"
#include "../branch.h"

static int recurse_lvl = 1;

static void debug_branch(branch br)
{
	int r = recurse_lvl * 2;
	switch (br.flags & BRANCH_TYPE_MASK) {
	case BRANCH_TYPE_VAR:
	{
		info_var *i = br.ptr; 
		debug("%*cVAR   '%s' '%s' '%s'", r, ' ', i->type, i->name, i->value);
		return;
	}
	case BRANCH_TYPE_C:
	{
		info_for *i = br.ptr;
		switch(br.flags & BRANCH_CTYPE_MASK) {
		case BRANCH_CTYPE_WHILE: debug("%*cWHILE '%s'", r, ' ', br.ptr); return;
		case BRANCH_CTYPE_IF:    debug("%*cIF    '%s'", r, ' ', br.ptr); return;
		case BRANCH_CTYPE_RET:   debug("%*cRET   '%s'", r, ' ', br.ptr); return;
		case BRANCH_CTYPE_FOR:   debug("%*cFOR   '%s' '%s' '%s'", r, ' ', i->it_var, i->it_expr, i->it_action); return;
		}
		debug("Invalid control type flag 0x%x", br.flags);
		abort();
	}
	default:
	{
		debug("Invalid type flag 0x%x", br.flags);
		abort();
	}
	}
}

static const char *var_types[] = {
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

static int parse(char **pptr, branch *root);

static int is_type_name(const char *str)
{
	for (size_t i = 0; i < sizeof(var_types) / sizeof(*var_types); i++) {
		if (strcmp(var_types[i], str) == 0)
			return 1;
	}
	return 0;
}

static int is_control_word(const char *str, char **pptr, branch *br)
{
	char *ptr = *pptr;
	const char *w[] = {
		"if",
		"while",
	};
	const int w_f[] = {
		BRANCH_CTYPE_IF,
		BRANCH_CTYPE_WHILE,
	};
	for (size_t i = 0; i < sizeof(w) / sizeof(*w); i++) {
		if (strcmp(w[i], str) == 0) {
			br->flags |= w_f[i];
			while (*ptr != '(')
				ptr++;
			ptr++;
			SKIP_WHITE(ptr);
			br->ptr = ptr;
			while (*ptr != ')')
				ptr++;
			*ptr = 0;
			*pptr = ptr + 1;
			br->flags |= BRANCH_TYPE_C;
			return 1;
		}
	}
	if (strcmp(str, "for") == 0) {
		info_for *f = br->ptr = malloc(sizeof(info_for));
		while (*ptr != '(')
			ptr++;
		ptr++;
		SKIP_WHITE(ptr);
		f->it_var = ptr;
		while (*ptr != ';')
			ptr++;
		*(ptr++) = 0;
		SKIP_WHITE(ptr);
		f->it_expr = ptr;
		while (*ptr != ';')
			ptr++;
		*(ptr++) = 0;
		SKIP_WHITE(ptr);
		f->it_action = ptr;
		while (*ptr != ')')
			ptr++;
		*ptr = 0;
		*pptr = ptr + 1;
		br->flags |= BRANCH_TYPE_C | BRANCH_CTYPE_FOR;
		return 1;
	}
	if (strcmp(str, "return") == 0) {
		SKIP_WHITE(ptr);
		br->ptr = ptr;
		while (*ptr != ';')
			ptr++;
		*ptr = 0;
		br->flags |= BRANCH_TYPE_C | BRANCH_CTYPE_RET;
		*pptr = ptr + 1;
		return 1;
	}
	return 0;
}

static int new_root(char **pptr, branch *root) {
	char *ptr = *pptr;
	branch br;
	br.flags = 0;
	br.ptr = malloc(1024 * sizeof(branch));
	while (*ptr != '}') {
		if (*ptr == 0) {
			fprintf(stderr, "Expected '}' ('%s')\n", *pptr);
			return -1;
		}
		if (*ptr == '{') {
			if (new_root(&ptr, &br) < 0)
				return -1;
		}
		ptr++;
	}
	*ptr = 0;
	while (1) {
		switch (parse(pptr, &br)) {
		case  1: break;
		case  0: goto done;
		case -1: return -1;
		}
	}
done:
	br.ptr = realloc(br.ptr, br.len * sizeof(branch));
	*pptr = ptr + 1;
	((branch *)root->ptr)[root->len++] = br;
	return 0; 
}

static int parse(char **pptr, branch *root)
{
	char c;
	char *type = NULL, *name;
	branch br;
	memset(&br, 0, sizeof(br));
	br.flags = BRANCH_ISLEAF;
	SKIP_WHITE(*pptr);
	char *ptr = *pptr;
	if (*ptr == 0)
		return 0;
	char *orgptr = ptr;
	while (!IS_WHITE(ptr) && *ptr != ';' && !IS_OPERATOR(*ptr) && *ptr != '(' && *ptr != '{')
		ptr++;
	if (!IS_WHITE(ptr)) {
		c = *ptr;
		*(ptr++) = 0;
	} else {
		*(ptr++) = 0;
		SKIP_WHITE(ptr);
		c = *ptr;
	}
	if (c == '{') {
		recurse_lvl++;
		ptr++;
		if (new_root(&ptr, root) < 0)
			return -1;
		recurse_lvl--;
		*pptr = ptr;
		return 1;
	} else if (is_type_name(orgptr)) {
		type = orgptr;
		orgptr = ptr;
		if (!('a' <= *ptr && *ptr <= 'z') && !('A' <= *ptr && *ptr <= 'Z') && *ptr != '_') {
			fprintf(stderr, "Invalid statement '%s'\n", ptr);
			return -1;
		}
		ptr++;
		while (!IS_WHITE(ptr) && *ptr != ';' && *ptr != '=')
			ptr++;
		if (!IS_WHITE(ptr)) {
			c = *ptr;
			*(ptr++) = 0;
		} else {
			*(ptr++) = 0;
			SKIP_WHITE(ptr);
			c = *ptr;
		}
	} else {
		int r = is_control_word(orgptr, &ptr, &br);
		if (r < 0)
			return -1;
		else if (r > 0)
			goto done;
	}
	ptr++;
	name = orgptr;
	if (c == ';' || IS_OPERATOR(c)) {
		info_var *f = br.ptr = malloc(sizeof(info_var));
		f->type = type;
		f->name = name;
		if (IS_OPERATOR(c)) {
			if (c != '=')
				ptr++;
			SKIP_WHITE(ptr);
			f->value = ptr;
			while (*ptr != ';')
				ptr++;
			*ptr = 0;
			if (c != '=') {
				char *tmp = malloc(0xFFFF);
				f->value = realloc(tmp, sprintf(tmp, "%s%c(%s)", f->name, c, f->value) + 1);
			}
		}
		br.flags |= BRANCH_TYPE_VAR;
		goto done;
	}
	SKIP_WHITE(ptr);
	while (*ptr != ';') {
		if (*ptr == 0) {
			debug("Expected semicolon '%s'", *pptr);
			return -1;
		}
		ptr++;
	}
done:
	debug_branch(br);
	((branch *)root->ptr)[root->len++] = br;
	*pptr = ptr + 1; // Skip ';'
	return 1;
}

int code_branch()
{
	global_func_branches = malloc(global_funcs_count * sizeof(branch));
	for (size_t i = 0; i < global_funcs_count; i++) {
		info_func *inf = global_funcs + i;
		debug("Branching '%s'", inf->name);
		char *ptr = inf->body;
		branch br;
		br.flags = 0;
		br.len = 0;
		br.ptr = malloc(1024 * sizeof(branch));
		while (1) {
			switch (parse(&ptr, &br)) {
			case  1: break;
			case  0: goto done;
			case -1: return -1;
			}
		}
	done:
		br.ptr = realloc(br.ptr, br.len * sizeof(branch));
		global_func_branches[i] = br;
	}
}
