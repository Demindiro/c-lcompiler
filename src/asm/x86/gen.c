#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../expr.h"
#include "../../table.h"
#include "../../code/read.h"
#include "../../code/branch.h"
#include "../../util.h"

#ifndef NDEBUG_ASM
#define debug_asm(msg, ...) printf(msg "\n", ##__VA_ARGS__);
#else
#define debug_asm(msg, ...) ""
#endif

typedef struct arg {
	char *type;
	const char *reg;
} arg;

const char *var_types[] = {
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

// Anybody has a good reference to general conventions of using the x86 registers? (like RISC-V has)
const char *arg_regs[] = {
	"rax", "rbx", "rcx", "rdx",
};

const char *temp_regs[] = {
	"r8" , "r9" , "r10", "r11",
	"r12", "r13", "r14", "r15",
};
unsigned int temp_regs_taken;


static int parse(branch *brs, size_t *index, table *tbl);

static const char *get_temp_reg_once()
{
	for (size_t i = 0; i < sizeof(temp_regs) / sizeof(*temp_regs); i++) {
		if (!(temp_regs_taken & (2 << i)))
			return temp_regs[i];
	}
	return NULL;
}

static const char *get_temp_reg()
{
	for (size_t i = 0; i < sizeof(temp_regs) / sizeof(*temp_regs); i++) {
		if (!(temp_regs_taken & (2 << i))) {
			temp_regs_taken |= 2 << i;
			return temp_regs[i];
		}
	}
	return NULL;
}

static int get_label(char *buf, char *prefix)
{
	static int counter = 0;
	if (prefix == NULL)
		prefix = "L";
	sprintf(buf, ".%s%d", prefix, counter++);
	return 0;
}

static void free_temp_reg(const char *reg) {
	size_t i = reg[2] - '0';
	temp_regs_taken &= ~i;
}

static size_t is_var_type(char *str)
{
	for (size_t i = 0; i < sizeof(var_types) / sizeof(*var_types); i++)
		if (strcmp(str, var_types[i]) == 0)
			return i;
	return -1;
}

static int print_op(int flags, const char *val)
{
	const char *treg, *extra_op0, *extra_op1;
	char buf[0x10];
	switch (flags & EXPR_OP_MASK) {
	case 0:             printf("\tmov\trax,%s\n" , val); break;
	case EXPR_OP_A_ADD: printf("\tadd\trax,%s\n" , val); break;
	case EXPR_OP_A_SUB: printf("\tsub\trax,%s\n" , val); break;
	case EXPR_OP_A_MUL: printf("\timul\trax,%s\n", val); break;
	case EXPR_OP_A_DIV:
	case EXPR_OP_A_REM:
		if (flags & EXPR_ISNUM) {
			treg = get_temp_reg_once();
			sprintf(buf, "\tmov\t%s,%s\n", treg, val);
			extra_op0 = buf;
		} else {
			treg = val;
			extra_op0 = "";
		}
		extra_op1 = ((flags & EXPR_OP_MASK) == EXPR_OP_A_REM) ? "\tmov\trax,rdx\n" : "";
		printf("\tpush\trdx\n"
		       "\txor\trdx,rdx\n"
		       "%s"
		       "\tidiv\t%s\n"
		       "%s"
		       "\tmov\trdx,%s\n"
		       "\tpop\trdx\n",
		       extra_op0, treg, extra_op1, treg);
		break;
	case EXPR_OP_C_GRT:
	case EXPR_OP_C_LST:
	case EXPR_OP_C_GET: printf("\tcmp\t%s,rax\n", val); break;
	case EXPR_OP_C_LET: printf("\tcmp\trax,%s\n", val); break;
	default: fprintf(stderr, "Invalid expression operator flags! 0x%x (This is a bug)\n", flags); return -1;
	}
	return 0;
}


static const char *get_val_or_reg(expr_branch root, table *tbl)
{
	if (root.flags & EXPR_ISNUM) {
		return root.val;
	} else { 
		arg a;
		if (table_get(tbl, &root.val, &a) < 0) {
			fprintf(stderr, "Undefined symbol: %s\n", root.val);
			return NULL;
		}
		return a.reg;
	}
}


static int _eval_expr(expr_branch root, table *tbl)
{
	const char *ar;
	if (root.flags & EXPR_ISLEAF) {
		debug_asm("; OP '%s' '%s'", debug_expr_op_to_str(root.flags), root.val);
		const char *arg = get_val_or_reg(root, tbl);
		if (arg == NULL)
			return -1;
		if (print_op(root.flags, arg) < 0)
			return -1;
	} else {
		debug_asm("; EXPR_START");
		const char *treg = get_temp_reg();
		printf("\tmov\t%s,rax\n", treg);
		for (size_t i = 0; i < root.len; i++) {
			if (_eval_expr(root.branches[i], tbl) < 0)
				return -1;
		}
		if (print_op(root.flags, treg) < 0)
			return -1;
		free_temp_reg(treg);
		debug_asm("; EXPR_END");
	}
	return 0;
}


static int eval_expr(expr_branch root, const char *dreg, table *tbl)
{
	int dreg_is_acc = strcmp(dreg, "rax") == 0;
	if (root.flags & EXPR_ISLEAF) {
		debug_asm("; OP '%s'", root.val);
		const char *arg = get_val_or_reg(root, tbl);
		if (arg == NULL)
			return -1;
		printf("\tmov\t%s,%s\n", dreg, arg);
	// Tip: if it is not a leaf, it has at least 2 branches
	} else {
		int offset;
		debug_asm("; EXPR2_START");
		const char *op;
		expr_branch br0 = root.branches[0], br1 = root.branches[1];
		switch (br1.flags & EXPR_OP_MASK) {
		case EXPR_OP_A_ADD: op = "add"; break;
		case EXPR_OP_A_SUB: op = "sub"; break;
		default:
			debug_asm("; No EXPR2 optimization");
			offset = 0;
			goto default_parse;
			break;	
		}
		// dreg will be overwritten anyways, so just use it
		// Constant can be used directly in most instructions, so check for those
		const char *t0;
		// Manually using push and pop to remove redundant pushes and pops in the middle
		if (!dreg_is_acc)
			printf("\tpush\trax\n");
		if (br0.flags & EXPR_ISLEAF && br0.flags & EXPR_ISNUM) {
			eval_expr(br1, "rax", tbl);
			t0 = br0.val;
		} else if (br1.flags & EXPR_ISLEAF && br1.flags & EXPR_ISNUM) {
			eval_expr(br0, "rax", tbl);
			t0 = br1.val;
		} else {
			eval_expr(br0, "rax", tbl);
			printf("\tmov\t%s,rax\n", dreg);
			const char *t0 = get_temp_reg();
			eval_expr(br1, "rax", tbl);
			free_temp_reg(t0);
		}
		printf("\t%s\t%s,%s\n", op, dreg, t0);
		offset = 2;
	default_parse:
		for (size_t i = offset; i < root.len; i++) { 
			if (_eval_expr(root.branches[i], tbl) < 0)
				return -1;
		}
		if (!dreg_is_acc) {
			printf("\tmov\t%s,rax\n"
			       "\tpop\trax\n", dreg);
		}
		printf("; EXPR2_END\n");
	}
	return 0;
}


static int print_jump_op(const char *lbl, int flags)
{
	const char *op;
	// Note: jumps are "reversed" because the branch after if should be executed
	switch(flags & EXPR_OP_MASK) {
		case EXPR_OP_C_GRT: op = "jle"; break;
		case EXPR_OP_C_LST: op = "jge"; break;
		case EXPR_OP_C_GET: op = "jl" ; break;
		case EXPR_OP_C_LET: op = "jg" ; break;
		default: fprintf(stderr, "print_jump_op\n"); exit(1); break;
	}
	printf("\t%s\t%s\n", op, lbl);
	return 0;
}

static int parse_if_expr(const char *lbl, expr_branch br, table *tbl)
{
	while (!(br.flags & EXPR_ISLEAF) && br.len == 1)
		br = br.branches[br.len - 1];
	if (br.flags & EXPR_ISLEAF) {

	} else if (br.len == 2) {
		expr_branch e0 = br.branches[0];
		expr_branch e1 = br.branches[1];
		int e1f = e1.flags;
		e1.flags &= ~EXPR_OP_MASK;
		const char *tr0 = get_temp_reg();
		eval_expr(e0, tr0, tbl);
		const char *tr1 = get_temp_reg();
		eval_expr(e1, tr1, tbl);
		printf("\tcmp\t%s,%s\n", tr0, tr1);
		free_temp_reg(tr0);
		free_temp_reg(tr1);
		if (print_jump_op(lbl, e1f) < 0)
			return -1;
	} else {
		debug("TODO (parse_if_expr)");
		exit(1);
	}
	return 0;
}

static int parse_control_word(branch *brs, size_t *index, table *tbl)
{
	branch br = brs[*index];
	if (br.flags & BRANCH_CTYPE_IF) {
		debug_asm("; IF\n");
		branch *brelse = &brs[*index + 2];
		if (!(brelse->flags & BRANCH_TYPE_C && brelse->flags & BRANCH_CTYPE_ELSE))
			brelse = NULL;
		char buf[16], buf2[16], *lbl = buf;
		get_label(buf, brelse == NULL ? "end" : "else");
		parse_if_expr(lbl, *br.expr, tbl);
		(*index)++;
		parse(brs, index, tbl);
		if (brelse != NULL) {
			get_label(buf2, "end");
			printf("\tjmp\t%s\n", buf2);
			printf("%s:\n", lbl);
			lbl = buf2;
			(*index) += 2;
			parse(brs, index, tbl);
		}
		printf("%s:\n", lbl);
	} else if (br.flags & BRANCH_CTYPE_RET) {
		debug_asm("; RET\n");
		eval_expr(*br.expr, "rax", tbl);
		printf("ret\n");
	} else if (br.flags & BRANCH_CTYPE_ELSE) {
		fprintf(stderr, "'ELSE' without 'IF' (should have been caught earlier, please file a bug report)\n");
		return -1;
	} else {
		fprintf(stderr, "Invalid control type flags! 0x%x (Please file a bug report)\n", br.flags);
		return -1;
	}
	return 0;
}

static int parse_var(branch br, table *tbl)
{
	info_var *inf = br.ptr;
	debug_asm("; VAR '%s' '%s'\n", inf->type, inf->name);
	arg a;
	if (inf->type != NULL) {
		a.type = inf->type;
		a.reg = get_temp_reg();
		if (table_add(tbl, &inf->name, &a) < 0) {
			fprintf(stderr, "Duplicate symbol: '%s'\n", inf->name);
			return -1;
		}
	} else if (table_get(tbl, &inf->name, &a) < 0) {
		fprintf(stderr, "Undefined symbol: '%s'\n", inf->name);
		return -1;
	}
	return eval_expr(inf->expr, a.reg, tbl);
}

static int parse(branch *brs, size_t *index, table *tbl)
{
	branch br = brs[*index];
	if (br.flags & BRANCH_ISLEAF) {
		if (br.flags & BRANCH_TYPE_C)
			return parse_control_word(brs, index, tbl);
		else if (br.flags & BRANCH_TYPE_VAR)
			return parse_var(br, tbl);
		fprintf(stderr, "Invalid type flags! 0x%x (Please file a bug report)\n", br.flags);
		return -1;
	} else {
		for (size_t i = 0; i < br.len; i++) {
			if (parse(br.ptr, &i, tbl) < 0)
				return -1;
		}
	}
	return 0;
}

static int convert_var(info_var *inf)
{
	printf("%s:\t", inf->name);
	if (strcmp(inf->type, "string") == 0) {
		printf("db\t\"%s\"\n", "TODO");
		printf("%s_len:\t equ $ - %s\n", inf->name, inf->name);
	} else if (strcmp(inf->type, "sbyte" ) == 0) {
		printf("db\t%s\n", "TODO");
	} else if (strcmp(inf->type, "sshort") == 0) {
		printf("dw\t%s\n", "TODO");
	} else if (strcmp(inf->type, "sint"  ) == 0) {
		printf("dd\t%s\n", "TODO");
	} else if (strcmp(inf->type, "slong" ) == 0) {
		printf("dq\t%s\n", "TODO");
	} else {
		printf("Unknown type: %s\n", inf->type);
		return -1;
	}
	printf("\n");
	return 0;
}

static int cmp_keys(const void *a, const void *b)
{
	char *const *x = a, *const *y = b;
	return strcmp(*x, *y);
}

static int convert_func(info_func *inf, branch root)
{
	if (inf->arg_count > sizeof(arg_regs) / sizeof(*arg_regs)) {
		fprintf(stderr, "Function '%s' has %d too many arguments (max: %d)\n",
		        inf->name, inf->arg_count - (sizeof(arg_regs) / sizeof(*arg_regs)),
			sizeof(arg_regs) / sizeof(*arg_regs));
		return -1;
	}
	table tbl;
	table_init(&tbl, sizeof(char *), sizeof(arg), cmp_keys);
	for (char i = 0; i < inf->arg_count; i++) {
		arg a;
		a.type = inf->arg_types[i];
		a.reg = arg_regs[i];
		if (table_add(&tbl, &inf->arg_names[i], &a) < 0) {
			fprintf(stderr, "Duplicate entry: %s\n", inf->arg_names[i]);
			return -1;
		}
	}
	printf("%s:\n", inf->name);
	for (size_t i = 0; i < root.len; i++) {
		if (parse(root.ptr, &i, &tbl) < 0)
			return -1;
	}
	printf("\n");
	table_free(&tbl);
	return 0;
}

int asm_gen()
{
	int r = 0;
	if (global_vars_count > 0) {
		printf("SECTION .data\n");
		for (size_t i = 0; i < global_vars_count; i++) {
			if (convert_var(&global_vars[i]) < 0)
				return -1;	
		}
	}
#ifdef __linux__
	printf("SECTION .text\n"
	       "global _start\n"
	       "_start:\n"
	       "\tcall\tmain\n"
	       "\tmov\trdi,rax\n"
	       "\tmov\trax,60\n"
	       "\tsyscall\n"
	       "\n");
#elif defined __MACH__
	// Fix a bug in dyld
	if (global_vars_count == 0)
		printf("SECTION .data\n"
		       "db 0\n"
		       "\n");
	printf("SECTION .text\n"
	       "global start\n"
	       "start:\n"
	       "\tcall\tmain\n"
	       "\tmov\trdi,rax\n"
	       "\tmov\trax,0x2000001\n"
	       "\tsyscall\n"
	       "\n");
#endif
	for (size_t i = 0; i < global_funcs_count; i++) {
		if (convert_func(&global_funcs[i], global_func_branches[i]) < 0)
			return -1;
	}
	return 0;
}
