#include "expr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "branch.h"


const char *debug_expr_op_to_str(int flags)
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

#ifndef NDEBUG

void debug_expr(expr_branch br, int lvl)
{
	if (br.flags & EXPR_ISLEAF) {
	        debug("%*cOP '%s', VAL '%s' (%s)",
	              lvl, ' ', debug_expr_op_to_str(br.flags), br.val, (br.flags & EXPR_ISNUM) ? "num" : "var");
	} else {
	        debug("%*cEXPR '%s'", lvl, ' ', debug_expr_op_to_str(br.flags));
	        for (size_t i = 0; i < br.len; i++)
	                debug_expr(br.branches[i], lvl + 2);
	}
}

#endif


static char *copy_var(char **pptr) {
	char *ptr = *pptr;
	if (*ptr == '+' || *ptr == '-')
		ptr++;
	if (*ptr == '0') {
		ptr++;
		if (*ptr == 'x') {
			ptr++;
			while (('0' <= *ptr && *ptr <= '9') ||
			       ('a' <= *ptr && *ptr <= 'f') ||
			       ('A' <= *ptr && *ptr <= 'F'))
				ptr++;
		} else if (*ptr == 'b') {
			ptr++;
			while (*ptr != '0' && *ptr != '1')
				ptr++;
		} else {
			while ('0' <= *ptr && *ptr <= '9')
				ptr++;
		}
	} else if ('1' <= *ptr && *ptr <= '9') {
		while ('0' <= *ptr && *ptr <= '9')
			ptr++;
	} else if (('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z') || *ptr == '_') {
		while (('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z') || *ptr == '_' ||
		       ('0' <= *ptr && *ptr <= '9'))
			ptr++;
	} else {
		fprintf(stderr, "Expected a number or variable name: '%s'\n", *pptr);
		return NULL;
	}
	if (strchr("\t \n" "+-""*-%""<>=!""&|^"")", *ptr) == NULL) {
		fprintf(stderr, "Unexpected character '%c': '%s'\n", *ptr, *pptr);
		return NULL;
	}
	size_t l = ptr - *pptr;
	char *str = malloc(l + 1);
	memcpy(str, *pptr, l);
	str[l] = 0;
	*pptr = ptr;
	return str;
}


static int _expr_parse(char **pptr, expr_branch *root)
{
	char *ptr = *pptr;
	root->len      = 0;
	root->branches = malloc(1024 * sizeof(char *));
	{
		expr_branch br;
		SKIP_WHITE(ptr);
		br.flags = EXPR_ISLEAF;
		br.val   = copy_var(&ptr);
		if (br.val == NULL)
			return -1;
		if ('0' <= br.val[0] && br.val[0] <= '9')
			br.flags |= EXPR_ISNUM;
		root->branches[root->len++] = br;
	}
	SKIP_WHITE(ptr);
	while (1) {
		expr_branch br;
		br.flags = EXPR_ISLEAF;
		switch (*ptr) {
		// Expression nesting
		case ')': *pptr = ptr + 1; return 0;
		// Arithemic
		case '+': br.flags |= EXPR_OP_A_ADD; break;
		case '-': br.flags |= EXPR_OP_A_SUB; break;
		case '*': br.flags |= EXPR_OP_A_MUL; break;
		case '/': br.flags |= EXPR_OP_A_DIV; break;
		case '%':
			if (*(++ptr) == '%') {
				br.flags |= EXPR_OP_A_MOD;
			} else {
				br.flags |= EXPR_OP_A_REM;
				ptr--;
			}
			break;
		// Comparison
		case '=':
			if (*(++ptr) == '=') {
				br.flags |= EXPR_OP_C_EQU;
			} else {
				fprintf(stderr, "Inline assignments are not supported: '%s'\n", *pptr);
				return -1;
			}
			break;
		// Somewhere in-between comparisons and bitwise
		case '>':
			if (*(++ptr) == '>') {
				br.flags |= EXPR_OP_B_RS;
			} else if (*ptr == '=') {
				br.flags |= EXPR_OP_C_GET;
			} else {
				br.flags |= EXPR_OP_C_GRT;
				ptr--;
			}
			break;
		case '<':
			if (*(++ptr) == '<') {
				br.flags |= EXPR_OP_B_LS;
			} else if (*ptr == '=') {
				br.flags |= EXPR_OP_C_LET;
			} else {
				br.flags |= EXPR_OP_C_LST;
				ptr--;
			}
			break;
		// Somewhere in-between comparisons and logicals
		case '!':
			if (*(++ptr) == '=') {
				br.flags |= EXPR_OP_C_EQU;
			} else {
				br.flags |= EXPR_OP_L_NOT;
				ptr--;
			}
			break;
		// Somewhere in-between logicals and bitwise
		case '&':
			if (*(++ptr) == '&') {
				br.flags |= EXPR_OP_L_AND;
			} else {
				br.flags |= EXPR_OP_B_AND;
				ptr--;
			}
			break;
		case '|':
			if (*(++ptr) == '|') {
				br.flags |= EXPR_OP_L_AND;
			} else {
				br.flags |= EXPR_OP_B_AND;
				ptr--;
			}
			break;
		// Logical
		case '~': br.flags |= EXPR_OP_B_XOR; break;
		case '^': br.flags |= EXPR_OP_B_XOR; break;
		default: fprintf(stderr, "Invalid symbol '%c': '%s'", *ptr, *pptr); return -1;
		}
		ptr++;
		SKIP_WHITE(ptr);
		if (*ptr == '(') {
			ptr++;
			br.flags &= ~EXPR_ISLEAF;
			if (_expr_parse(&ptr, &br) < 0)
				return -1;
		} else {
			br.val = copy_var(&ptr);
			if (br.val == NULL)
				return -1;
			if ('0' <= br.val[0] && br.val[0] <= '9')
				br.flags |= EXPR_ISNUM;
		}
		root->branches[root->len++] = br;
	}
	root->branches = realloc(root->branches, root->len * sizeof(char *));
	return 0;
}

int expr_parse(char **pptr, expr_branch *root)
{
	root->flags = 0;
	return _expr_parse(pptr, root);
}
