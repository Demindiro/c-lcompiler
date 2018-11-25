#include "code/branch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/string.h"
#include "code/read.h"
#include "code/split.h"
#include "util.h"
#include "branch.h"
#include "expr.h"


#define error(msg, row, col, ...) \
	fprintf(stderr, msg " (%d:%d)\n", \
	row, col, ##__VA_ARGS__)
#define return_error(msg, row, col, ...) { error(msg, row, col, ##__VA_ARGS__); return -1; }


static int parse(lines_t line, size_t *index, branch *root);


static int new_root(lines_t lines, branch *root) {
	branch br;
	br.len = 0;
	br.flags = 0;
	br.ptr = malloc(1024 * sizeof(branch));
	for (size_t i = 0; i < lines.count; i++) {
		switch (parse(lines, &i, &br)) {
		case  1: break;
		case  0: goto done;
		case -1: return -1;
		}
	}
done:
	br.ptr = realloc(br.ptr, br.len * sizeof(branch));
	((branch *)root->ptr)[root->len++] = br;
	return 0;
}


static int eval_ctrl_expr(branch *br, string str, size_t i, line_t line)
{
	string e = string_copy(str, i + 1, str->len);
	br->expr = malloc(sizeof(*br->expr));
	if (expr_parse(e, br->expr) < 0)
		return_error("Could not parse expression", line.row, line.col);
	free(e);
	return 0;
}


static int parse(lines_t lines, size_t *index, branch *root)
{
	line_t line = lines.lines[*index];
	string str = line.str;
	size_t i = 0;
	string type = NULL, name;
	branch br;
	memset(&br, 0, sizeof(br));
	br.flags = BRANCH_ISLEAF;
	if (str->buf[0] == '{') {
		size_t i = *index + 1;
		// No need to check range, otherwise the compilation would have
		// failed earlier.
		while (lines.lines[*index].str->buf[0] != '}')
			(*index)++;
		lines_t l = { .count = *index - i, .lines = lines.lines + i };
		if (new_root(l, root) < 0)
			return -1;
		return 1;
	}

	while (!IS_VAR_CHAR(str->buf[i]))
		i++;
	size_t j = i;
	while (IS_VAR_CHAR(str->buf[i]))
		i++;

	// Control type
	switch (i) {
	case 2:
		if (memcmp(str->buf, "if", 2) == 0) {
			if (eval_ctrl_expr(&br, str, i, line) < 0)
				return -1;
			br.flags |= BRANCH_TYPE_C | BRANCH_CTYPE_IF;
			goto done;
		}
		break;
	case 3:
		if (memcmp(str->buf, "for", 3) == 0) {
			return_error("'for' is not implemented", line.row, line.col);
			br.flags |= BRANCH_TYPE_C | BRANCH_CTYPE_FOR;
			goto done;
		}
		break;
	case 4:
		if (memcmp(str->buf, "else", 4) == 0) {
			if (str->len > 4)
				return_error("Unexpected characters", line.row, line.col + 4);
			br.flags |= BRANCH_TYPE_C | BRANCH_CTYPE_ELSE;
			goto done;
		}
		break;
	case 5:
		if (memcmp(str->buf, "while", 5) == 0) {
			if (eval_ctrl_expr(&br, str, i, line) < 0)
				return -1;
			br.flags |= BRANCH_TYPE_C | BRANCH_CTYPE_WHILE;
			goto done;
		}
		break;
	case 6:
		if (memcmp(str->buf, "return", 6) == 0) {
			if (eval_ctrl_expr(&br, str, i, line) < 0)
				return -1;
			br.flags |= BRANCH_TYPE_C | BRANCH_CTYPE_RET;
			goto done;
		}
		break;
	}

	br.flags |= BRANCH_TYPE_VAR;
	char c = str->buf[i];
	if (str->len <= i && c != ' ')
		return_error("Expected expression", line.row, line.col + (int)i);
	if (c == ' ') {
		type = string_copy(str, j, i);
		i++;
		j = i;
		while (IS_VAR_CHAR(str->buf[i]))
			i++;
	}
	name = string_copy(str, j, i);
	if (str->len == i)
		goto done;

	info_var *f = br.ptr = malloc(sizeof(info_var));
	f->type = type;
	f->name = name;
	f->expr.flags = 0;
	if (strchr("=+-*/%<>", str->buf[i]) != NULL) {
		size_t j = i;
		if (strchr("+-*/%", str->buf[i]) != NULL) {
			i++;
		} else if (strchr("<>", str->buf[i]) != NULL) {
			i++;
			if (strchr("<>", str->buf[i]) != NULL)
				i++;
		}
		if (str->buf[i] != '=')
			return_error("Expected '='", line.row, line.col);
		i++;
		string e = string_copy(str, i, str->len);
		if (j != i - 1) {
			string c[3] = { name, string_copy(str, j, i - 1), e };
			e = string_concat(c, 2);
			free(c[1]);
			free(c[2]);
		}
		if (expr_parse(e, &f->expr) < 0)
			return_error("Could not parse expression", line.row, line.col);
		free(e);
	}
done:
	((branch *)root->ptr)[root->len++] = br;
	return 1;
}


int code_branch()
{
	global_func_branches = malloc(global_funcs_count * sizeof(branch));
	for (size_t i = 0; i < global_funcs_count; i++) {
		info_func *inf = global_funcs + i;
		debug("Branching '%s'", inf->name->buf);
		branch br;
		br.flags = 0;
		br.len = 0;
		br.ptr = malloc(1024 * sizeof(branch));
		for(size_t j = 0; j < inf->lines->count; j++) {
			switch (parse(*inf->lines, &j, &br)) {
			case  1: break;
			case  0: goto done;
			case -1: return -1;
			}
		}
	done:
		debug_branch(br);
		br.ptr = realloc(br.ptr, br.len * sizeof(branch));
		global_func_branches[i] = br;
		debug("");
	}
	return 0;
}
