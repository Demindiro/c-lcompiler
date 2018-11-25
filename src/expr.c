#include "expr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <include/string.h>
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
	              lvl, ' ', debug_expr_op_to_str(br.flags), br.val->buf, (br.flags & EXPR_ISNUM) ? "num" : "var");
	} else {
	        debug("%*cEXPR '%s'", lvl, ' ', debug_expr_op_to_str(br.flags));
	        for (size_t i = 0; i < br.len; i++)
	                debug_expr(br.branches[i], lvl + 2);
	}
}

#endif


static int _skip_while(string str, size_t *i, char *delims)
{
	while (1) {
		if (str->len >= *i)
			return -1;
		char c = str->buf[*i];
		for (char *d = delims; *d != 0; d++) {
			if (c == *d)
				goto next;
		}
		return 0;
	next:;
	}
}


static int _skip_until(string str, size_t *i, char *delims)
{
	while (1) {
		if (str->len >= *i)
			return -1;
		char c = str->buf[*i];
		for (char *d = delims; *d != 0; d++) {
			if (c == *d)
				return 0;
		}
	}
}


static string copy_var(string str, size_t *i) {
	char *ptr = str->buf + *i;
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
		fprintf(stderr, "Expected a number or variable name: '%s'\n", str->buf);
		return NULL;
	}
	if (strchr("\t \n" "+-""*/%""<>=!""&|^"")", *ptr) == NULL) {
		fprintf(stderr, "Unexpected character '%c': '%s'\n", *ptr, str->buf);
		return NULL;
	}
	size_t j = ptr - str->buf;
	string v = string_copy(str, *i, j);
	*i = ptr - str->buf;
	return v;
}


static int _expr_parse(string str, expr_branch *root)
{
	size_t  i = 0;
	root->len = 0;
	root->branches = malloc(1024 * sizeof(char *));
	{
		expr_branch br;
		br.flags = EXPR_ISLEAF;
		br.val   = copy_var(str, &i);
		if (br.val == NULL)
			return -1;
		if ('0' <= br.val->buf[0] && br.val->buf[0] <= '9')
			br.flags |= EXPR_ISNUM;
		root->branches[root->len++] = br;
	}
	while (1) {
		if (i >= str->len)
			return 0;
		expr_branch br;
		br.flags = EXPR_ISLEAF;
		switch (str->buf[i]) {
		// Arithemic
		case '+': br.flags |= EXPR_OP_A_ADD; break;
		case '-': br.flags |= EXPR_OP_A_SUB; break;
		case '*': br.flags |= EXPR_OP_A_MUL; break;
		case '/': br.flags |= EXPR_OP_A_DIV; break;
		case '%':
			if (str->buf[++i] == '%') {
				br.flags |= EXPR_OP_A_MOD;
			} else {
				br.flags |= EXPR_OP_A_REM;
				i--;
			}
			break;
		// Comparison
		case '=':
			if (str->buf[++i] == '=') {
				br.flags |= EXPR_OP_C_EQU;
			} else {
				fprintf(stderr, "Inline assignments are not supported: '%s'\n", str->buf);
				return -1;
			}
			break;
		// Somewhere in-between comparisons and bitwise
		case '>':
			if (str->buf[++i] == '>') {
				br.flags |= EXPR_OP_B_RS;
			} else if (str->buf[i] == '=') {
				br.flags |= EXPR_OP_C_GET;
			} else {
				br.flags |= EXPR_OP_C_GRT;
				i--;
			}
			break;
		case '<':
			if (str->buf[++i] == '<') {
				br.flags |= EXPR_OP_B_LS;
			} else if (str->buf[i] == '=') {
				br.flags |= EXPR_OP_C_LET;
			} else {
				br.flags |= EXPR_OP_C_LST;
				i--;
			}
			break;
		// Somewhere in-between comparisons and logicals
		case '!':
			if (str->buf[++i] == '=') {
				br.flags |= EXPR_OP_C_EQU;
			} else {
				br.flags |= EXPR_OP_L_NOT;
				i--;
			}
			break;
		// Somewhere in-between logicals and bitwise
		case '&':
			if (str->buf[++i] == '&') {
				br.flags |= EXPR_OP_L_AND;
			} else {
				br.flags |= EXPR_OP_B_AND;
				i--;
			}
			break;
		case '|':
			if (str->buf[++i] == '|') {
				br.flags |= EXPR_OP_L_AND;
			} else {
				br.flags |= EXPR_OP_B_AND;
				i--;
			}
			break;
		// Logical
		case '~': br.flags |= EXPR_OP_B_XOR; break;
		case '^': br.flags |= EXPR_OP_B_XOR; break;
		default: fprintf(stderr, "Invalid symbol '%c': '%s'\n", str->buf[i], str->buf); return -1;
		}
		i++;
		if (str->buf[i] == '(') {
			i++;
			size_t j = i;
			while (str->buf[i] != ')')
				i++;
			br.flags &= ~EXPR_ISLEAF;
			string s = string_create(str->buf + j, i - j); 
			if (_expr_parse(s, &br) < 0)
				return -1;
			i++;
		} else {
			br.val = copy_var(str, &i);
			if (br.val == NULL)
				return -1;
			if ('0' <= br.val->buf[0] && br.val->buf[0] <= '9')
				br.flags |= EXPR_ISNUM;
		}
		root->branches[root->len++] = br;
	}
	root->branches = realloc(root->branches, root->len * sizeof(char *));
	return 0;
}

int expr_parse(string str, expr_branch *root)
{
	root->flags = 0;
	return _expr_parse(str, root);
}
