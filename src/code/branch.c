#include "branch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "read.h"
#include "../util.h"
#include "../branch.h"
#include "../expr.h"


static int recurse_lvl = 1;


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

// ====

static const char *expr_op_to_str(int flags)
{
	switch (flags & EXPR_OP_MASK) {
	case 0:              return "none";
	// Arithemic
	case EXPR_OP_A_ADD:  return "+";
	case EXPR_OP_A_SUB:  return "-";
	case EXPR_OP_A_MUL:  return "*";
	case EXPR_OP_A_DIV:  return "/";
	case EXPR_OP_A_REM:  return "%";
	case EXPR_OP_A_MOD:  return "%%";
	// Comparison
	case EXPR_OP_C_EQU:  return "==";
	case EXPR_OP_C_NEQ:  return "!=";
	case EXPR_OP_C_GRT:  return ">";
	case EXPR_OP_C_LST:  return "<";
	case EXPR_OP_C_GET:  return ">=";
	case EXPR_OP_C_LET:  return "<=";
	// Logical
	case EXPR_OP_L_NOT:  return "!";
	case EXPR_OP_L_AND:  return "&&";
	case EXPR_OP_L_OR :  return "||";
	// Bitwise
	case EXPR_OP_B_NOT:  return "~";
	case EXPR_OP_B_AND:  return "&";
	case EXPR_OP_B_OR :  return "|";
	case EXPR_OP_B_XOR:  return "^";
	case EXPR_OP_B_LS :  return "<<";
	case EXPR_OP_B_RS :  return ">>";
	}
	fprintf(stderr, "Invalid expression operator flag! 0x%x (This is a bug)\n", flags);
	exit(1);
}

static void debug_expr(expr_branch br, int lvl)
{
	if (br.flags & EXPR_ISLEAF) {
		debug("%*cOP '%s', VAL '%s' (%s)",
                      lvl, ' ', expr_op_to_str(br.flags), br.val, (br.flags & EXPR_ISNUM) ? "num" : "var");
	} else {
		debug("%*cEXPR '%s'", lvl, ' ', expr_op_to_str(br.flags));
		for (size_t i = 0; i < br.len; i++)
			debug_expr(br.branches[i], lvl + 2);
	}
}


static void debug_branch(branch br)
{
	int r = recurse_lvl * 2;
	switch (br.flags & BRANCH_TYPE_MASK) {
	case BRANCH_TYPE_VAR:
	{
		info_var *i = br.ptr; 
		debug("%*cVAR '%s' '%s'", r, ' ', i->type, i->name);
		debug_expr(i->expr, r + 2);
	}
	break;
	case BRANCH_TYPE_C:
	{
		info_for *i = br.ptr;
		switch(br.flags & BRANCH_CTYPE_MASK) {
		case BRANCH_CTYPE_WHILE: debug("%*cWHILE", r, ' '); break;
		case BRANCH_CTYPE_IF:    debug("%*cIF"   , r, ' '); break;
		case BRANCH_CTYPE_ELSE:  debug("%*cELSE" , r, ' '); return;
		case BRANCH_CTYPE_RET:   debug("%*cRET"  , r, ' '); break;
		case BRANCH_CTYPE_FOR:
			debug("%*cFOR   '%s' '%s'", r, ' ', i->var, i->action);
			debug_expr(i->expr, r + 2);
			return;
		default:
			debug("Invalid control type flag 0x%x", br.flags);
			exit(1);
		}
		debug_expr(*br.expr, r + 2);
	}
	break;
	default:
	{
		debug("Invalid type flag 0x%x", br.flags);
		exit(1);
	}
	}
}

// ====

static int parse(char **pptr, branch *root);

// ====

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
			br->expr = malloc(sizeof(*br->expr));
			if (expr_parse(&ptr, br->expr) < 0)
				return -1;
			*pptr = ptr;
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
		f->var = ptr;
		while (*ptr != ';')
			ptr++;
		*(ptr++) = 0;
		SKIP_WHITE(ptr);
		if (expr_parse(&ptr, &f->expr))
			return -1;
		while (*ptr != ';')
			ptr++;
		*(ptr++) = 0;
		SKIP_WHITE(ptr);
		f->action = ptr;
		while (*ptr != ')')
			ptr++;
		*ptr = 0;
		*pptr = ptr + 1;
		br->flags |= BRANCH_TYPE_C | BRANCH_CTYPE_FOR;
		return 1;
	}
	if (strcmp(str, "return") == 0) {
		SKIP_WHITE(ptr);
		while (*ptr != ';')
			ptr++;
		*ptr = ')';
		br->expr = malloc(sizeof(*br->expr));
		if (expr_parse(pptr, br->expr))
			return -1;
		br->flags |= BRANCH_TYPE_C | BRANCH_CTYPE_RET;
		return 1;
	}
	if (strcmp(str, "else") == 0) {
		br->flags |= BRANCH_TYPE_C | BRANCH_CTYPE_ELSE;
		*pptr = ptr - 1; // There is no character (')', ';' ...) to skip, so no +1
		                 // TODO figure out why -1 fixes things with semicolons
		return 1;
	}
	return 0;
}


static int new_root(char **pptr, branch *root) {
	char *ptr = *pptr;
	branch br;
	br.len = 0;
	br.flags = 0;
	br.ptr = malloc(1024 * sizeof(branch));
	int c = 0;
	while (1) {
		if (*ptr == '{') {
			c++;
		} else if (*ptr == '}') {
			c--;
		} else if (*ptr == 0) {
			fprintf(stderr, "Expected '}' ('%s')\n", *pptr);
			return -1;
		}
		if (c < 0)
			break;
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
	if (*ptr == '{') {
		recurse_lvl++;
		ptr++;
		if (new_root(&ptr, root) < 0)
			return -1;
		recurse_lvl--;
		*pptr = ptr;
		return 1;
	} 	
	char *orgptr = ptr;
	while (!IS_WHITE(ptr) && *ptr != ';' && !IS_OPERATOR(*ptr) && *ptr != '(')
		ptr++;
	if (!IS_WHITE(ptr)) {
		c = *ptr;
		*(ptr++) = 0;
	} else {
		*(ptr++) = 0;
		SKIP_WHITE(ptr);
		c = *ptr;
	}
	if (is_type_name(orgptr)) {
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
		f->expr.flags = 0;
		if (IS_OPERATOR(c)) {
			orgptr = ptr;
			int count = 0;
			while (*ptr != ';') {
				if (*ptr == '(')
					count++;
				else if (*ptr == ')')
					count--;
				if (count < 0) {
					fprintf(stderr, "Unexpected ')': '%s'\n", orgptr);
					return -1;
				}
				ptr++;
			}
			if (count > 0) {
				fprintf(stderr, "Missing ')': '%s'\n", orgptr);
				return -1;
			}
			*(ptr++) = ')'; // ')' indicates the end of an expression
			if (c != '=') {
				orgptr += 2;
				SKIP_WHITE(orgptr);
				char buf[0x10000], *d = buf;
				*(ptr-1) = 0;
				sprintf(buf, "%s%c(%s))", f->name, c, orgptr);
				d = buf;
				if (expr_parse(&d, &f->expr) < 0)
					return -1;
			} else {
				if (expr_parse(&orgptr, &f->expr) < 0)
					return -1;
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
		debug("");
	}
}