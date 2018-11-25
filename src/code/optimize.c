#include "code/optimize.h"
#include <stdio.h>
#include <stdlib.h>
#include "code/read.h"
#include "code/branch.h"
#include "expr.h"
#include "util.h"

static int _optimize_expr(expr_branch *root)
{
	if (root->len == 1) {
		void *tmp = root->branches;
		*root = root->branches[0];
		free(tmp);
	} else {
		for (size_t i = 0; i < root->len; i++) {
			expr_branch *br = &root->branches[i];
			if (!(br->flags & EXPR_ISLEAF)) {
				if (_optimize_expr(br) < 0)
					return -1;
			}
		}
	}
	return 0;
}

static int optimize_expr(branch *br)
{
	expr_branch *root;
	switch (br->flags & BRANCH_TYPE_MASK) {
	case BRANCH_TYPE_VAR:
		root = &((info_var *)br->ptr)->expr;
		break;
	case BRANCH_TYPE_C:
		if (br->flags & BRANCH_CTYPE_ELSE)
			return 0; // 'else' doesn't have an expression
		root = br->expr;
		break;
	default:
		fprintf(stderr, "Invalid type flags! 0x%x\n", br->flags);
		return -1;
	}
	return _optimize_expr(root);
}

static int optimize_branch(branch *root)
{
	if (root->len == 0) {
		return 0; // Removing the function may break other functions,
		          // so just don't do anyhting
	} else if (root->len == 1) {
		void *tmp = root->branches;
		*root = root->branches[0];
		free(tmp);
		if (optimize_expr(root) < 0)
			return -1;
	} else {
		for (size_t i = 0; i < root->len; i++) {
			branch *br = &root->branches[i];
			if (!(br->flags & BRANCH_ISLEAF)) {
				if (optimize_branch(br) < 0)
					return -1;
			} else {
				if (optimize_expr(br) < 0)
					return -1;
			}
		}
	}
	return 0;
}

int code_optimize()
{
	for (size_t i = 0; i < global_funcs_count; i++) {
		debug("Optimizing %s:", global_funcs[i].name->buf);
		if (optimize_branch(&global_func_branches[i]) < 0)
			return 0;
		debug_branch(global_func_branches[i]);
	}
	return 0;
}
