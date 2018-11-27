#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/string.h"
#include "expr.h"
#include "table.h"
#include "code/read.h"
#include "code/branch.h"
#include "util.h"
#include "x86.h"
#include "optimize.h"


static int parse(assembly_t *assm, branch *brs, size_t *index, table *tbl);


static enum REG get_temp_reg_once()
{
	for (size_t i = 0; i < temp_regs_count; i++) {
		if (!(temp_regs_taken & (1 << i)))
			return temp_regs[i];
	}
	return -1;
}


static enum REG get_temp_reg()
{
	for (size_t i = 0; i < temp_regs_count; i++) {
		if (!(temp_regs_taken & (1 << i))) {
			temp_regs_taken |= 1 << i;
			return temp_regs[i];
		}
	}
	return -1;
}


static string get_label(const char *prefix)
{
	static int counter = 0;
	if (prefix == NULL)
		prefix = "L";
	char buf[64];
	sprintf(buf, ".%s%d", prefix, counter++);
	return string_create(buf);
}


static void free_temp_reg(enum REG reg) {
	for (size_t i = 0; i < temp_regs_count; i++) {
		if (temp_regs[i] == reg) {
			temp_regs_taken &= ~(1 << i);
			return;
		}
	}
}


static int parse_op(assembly_t *assm, int flags, enum REG dest, enum REG src, size_t val)
{
	enum OP op;
	switch (flags & EXPR_OP_MASK) {
	case 0:             op = MOV; break;
	case EXPR_OP_A_ADD: op = ADD; break;
	case EXPR_OP_A_SUB: op = SUB; break;
	case EXPR_OP_A_MUL: op = IMUL; break;
	case EXPR_OP_A_DIV:
	case EXPR_OP_A_REM: op = IDIV; break;
	case EXPR_OP_C_GRT: /* TODO */; break;
	case EXPR_OP_C_LST: /* TODO */; break;
	case EXPR_OP_C_GET:; enum REG tmp = val; val = dest; dest = tmp;
	case EXPR_OP_C_LET: op = CMP; break;
	default: fprintf(stderr, "Invalid expression operator flags! 0x%x (This is a bug)\n", flags); return -1;
	}
	if (op == IDIV) {
		if (dest != AX) {
			op_t push = { .op = PUSH, .src = AX };
			op_t mov  = { .op = MOV, .dest = AX, .src = dest };
			assm_add_op(assm, &push);
			assm_add_op(assm, &mov );
		}
		op_t push = { .op = PUSH, .src  = DX };
		op_t zero = { .op = XOR , .dest = DX, .src = DX };
		assm_add_op(assm, &push);
		assm_add_op(assm, &zero);
		if (flags & EXPR_ISNUM) {
			src = get_temp_reg_once();
			op_t mov = { .op = MOV, .dest = src, .src = { .val = val, .is_num = 1 }};
			assm_add_op(assm, &mov);
		}
		op_t div  = { .op = op  , .src  = src };
		op_t pop  = { .op = POP , .dest = DX };
		assm_add_op(assm, &div);
		assm_add_op(assm, &pop);
		if (dest != AX) {
			op_t mov = { .op = MOV, .dest = dest, .src = AX };
			op_t pop = { .op = POP, .dest = AX };
			assm_add_op(assm, &mov);
			assm_add_op(assm, &pop);
		}
	} else if (op == IMUL) {
		op_t o = { .op = op, .dest = dest, .src = (flags & EXPR_ISNUM) ? val : src };
		assm_add_op(assm, &o);
	} else {
		op_t o = { .op = op, .dest = dest };
		if (flags & EXPR_ISNUM) {
			o.src.val = val;
			o.src.is_num = 1;
		} else {
			o.src.reg = src;
		}
		assm_add_op(assm, &o);
	}
	return 0;
}


static int _eval_expr(assembly_t *assm, enum REG dreg, expr_branch root, table *tbl)
{
	const char *ar;
	if (root.flags & EXPR_ISLEAF) {
		char buf[256];
		sprintf(buf, "EXPR %s %s", debug_expr_op_to_str(root.flags), root.val->buf);
		op_t *comm = assm->end;
		size_t index = assm->count;
		enum REG sreg;
		size_t val;
		if (root.flags & EXPR_ISNUM) {
			val = atoi(root.val->buf);
		} else {
			sreg = (enum REG)table_get(tbl, root.val);
			if ((void *)sreg == NULL) {
				fprintf(stderr, "Undefined symbol: %s", root.val->buf);
				return -1;
			}
		}
		if (parse_op(assm, root.flags, dreg, sreg, val) < 0)
			return -1;
		comm->next->comment = string_create(buf);
	} else {
		op_t comm = { .comment = string_create("EXPR_START") };
		assm_add_op(assm, &comm);
		enum REG treg = get_temp_reg();
		op_t op = { .op = PUSH, .src = AX };
		assm_add_op(assm, &op);
		for (size_t i = 0; i < root.len; i++) {
			if (_eval_expr(assm, dreg, root.branches[i], tbl) < 0)
				return -1;
		}
		if (parse_op(assm, root.flags, dreg, treg, -1) < 0)
			return -1;
		free_temp_reg(treg);
		op.op   = POP;
		op.dest = op.src;
		assm_add_op(assm, &op);
		comm.comment = string_create("EXPR_END"); 
		assm_add_op(assm, &comm);
	}
	return 0;
}


static int eval_expr(assembly_t *assm, expr_branch root, enum REG dreg, table *tbl)
{
	if (root.flags & EXPR_ISLEAF) {
		string s[] = { string_create("OP "), root.val };
		op_t op = { .op = MOV, .dest = dreg, .comment = string_concat(s, 2) };
		free(s[0]);
		op.src.is_num = !!(root.flags & EXPR_ISNUM);
		if (op.src.is_num) {
			op.src.val = atoi(root.val->buf);
		} else {
			op.src.reg = (enum REG)table_get(tbl, root.val);
			if ((void *)op.src.reg == NULL) {
				fprintf(stderr, "'%s' is not defined\n", root.val->buf);
				return -1;
			}
		}
		assm_add_op(assm, &op);
	// Tip: if it is not a leaf, it has at least 2 branches
	} else {

		int offset, expr2 = 1;
		op_t op = { .dest = dreg }, tmp;// = { .dest = dreg };

		expr_branch br0 = root.branches[0], br1 = root.branches[1];
		switch (br1.flags & EXPR_OP_MASK) {
		case EXPR_OP_A_ADD: op.op = ADD; break;
		case EXPR_OP_A_SUB: op.op = SUB; break;
		default:
			offset = 0;
			expr2  = 0;
			goto default_parse;
		}

		op_t comm = { .comment = string_create("EXPR2_START") };
		if (dreg != AX)
			assm_insert_op(assm, assm->count-1, &comm);
		else
			assm_add_op(assm, &comm);

		eval_expr(assm, br1, AX, tbl);
		op.src.reg = get_temp_reg();
		op_t o = { .op = MOV, .dest = op.src, .src = AX };
		assm_add_op(assm, &o);
		eval_expr(assm, br0, AX, tbl);
		free_temp_reg(op.src.reg);

		op_t mov = { .op = MOV, .dest = dreg, .src = AX };
		assm_add_op(assm, &mov);
		assm_add_op(assm, &op);
		offset = 2;

	default_parse:
		for (size_t i = offset; i < root.len; i++) { 
			if (_eval_expr(assm, dreg, root.branches[i], tbl) < 0)
				return -1;
		}

		if (expr2) {
			op_t comm = { .comment = string_create("EXPR2_END") };
			assm_add_op(assm, &comm);
		}
	}
	return 0;
}


static int print_jump_op(assembly_t *assm, string lbl, int flags)
{
	op_t op = { .label = lbl };
	// Note: jumps are "reversed" because the branch after if should be executed
	switch(flags & EXPR_OP_MASK) {
		case EXPR_OP_C_GRT: op.op = JLE; break;
		case EXPR_OP_C_LST: op.op = JGE; break;
		case EXPR_OP_C_GET: op.op = JL ; break;
		case EXPR_OP_C_LET: op.op = JG ; break;
		default: fprintf(stderr, "print_jump_op\n"); exit(1); break;
	}
	assm_add_op(assm, &op);
	return 0;
}

static int parse_if_expr(assembly_t *assm, string lbl, expr_branch br, table *tbl)
{
	while (!(br.flags & EXPR_ISLEAF) && br.len == 1)
		br = br.branches[br.len - 1];
	if (br.flags & EXPR_ISLEAF) {
		/* TODO */
	} else if (br.len == 2) {
		expr_branch e0 = br.branches[0];
		expr_branch e1 = br.branches[1];
		int e1f = e1.flags;
		e1.flags &= ~EXPR_OP_MASK;
		enum REG tr0 = get_temp_reg();
		eval_expr(assm, e0, tr0, tbl);
		enum REG tr1 = get_temp_reg();
		eval_expr(assm, e1, tr1, tbl);
		op_t cmp = { .op = CMP, .src = tr1, .dest = tr0 };
		assm_add_op(assm, &cmp);
		free_temp_reg(tr0);
		free_temp_reg(tr1);
		if (print_jump_op(assm, lbl, e1f) < 0)
			return -1;
	} else {
		debug("TODO (parse_if_expr)");
		exit(1);
	}
	return 0;
}


static int parse_control_word(assembly_t *assm, branch *brs, size_t *index, table *tbl)
{
	branch br = brs[*index];
	if (br.flags & BRANCH_CTYPE_IF) {
		op_t comm = { .comment = string_create("IF") };
		assm_add_op(assm, &comm);
		branch *brelse = &brs[*index + 2];
		if (!(brelse->flags & BRANCH_TYPE_C && brelse->flags & BRANCH_CTYPE_ELSE))
			brelse = NULL;
		string lbl = get_label(brelse == NULL ? "end" : "else");
		parse_if_expr(assm, lbl, *br.expr, tbl);
		(*index)++;
		parse(assm, brs, index, tbl);
		if (brelse != NULL) {
			string lblelse = get_label("end");
			op_t jmp = { .op = JMP, .label = lblelse };
			op_t l   = { .label = lbl, .is_label = 1 };
			assm_add_op(assm, &jmp);
			assm_add_op(assm, &l);
			lbl = lblelse;
			(*index) += 2;
			parse(assm, brs, index, tbl);
		}
		op_t l = { .label = lbl, .is_label = 1 };
		assm_add_op(assm, &l);
	} else if (br.flags & BRANCH_CTYPE_RET) {
		op_t comm = { .comment = string_create("RET") };
		assm_add_op(assm, &comm);
		eval_expr(assm, *br.expr, AX, tbl);
		op_t op = { .op = RET };
		assm_add_op(assm, &op);
	} else if (br.flags & BRANCH_CTYPE_ELSE) {
		fprintf(stderr, "'ELSE' without 'IF' (should have been caught earlier, please file a bug report)\n");
		return -1;
	} else {
		fprintf(stderr, "Invalid control type flags! 0x%x (Please file a bug report)\n", br.flags);
		return -1;
	}
	return 0;
}


static int parse_var(assembly_t *assm, branch br, table *tbl)
{
	info_var *inf = br.ptr;
	string s[] = { string_create("VAR "), inf->type ? inf->type : string_create(""), string_create(" "), inf->name };
	op_t comm = { .comment = string_concat(s, 4) };
	assm_add_op(assm, &comm);
	free(s[0]);
	free(s[2]);
	if (inf->type == NULL)
		free(s[1]);
	enum REG reg;
	if (inf->type != NULL) {
		reg = get_temp_reg();
		if (table_add(tbl, inf->name, (void *)reg) < 0) {
			fprintf(stderr, "Duplicate symbol: '%s'\n", inf->name->buf);
			return -1;
		}
	} else {
		reg = (enum REG)table_get(tbl, inf->name); 
		if ((void *)reg == NULL) {
			fprintf(stderr, "Undefined symbol: '%s'\n", inf->name->buf);
			return -1;
		}
	}
	return eval_expr(assm, inf->expr, reg, tbl);
}


static string referenced_funcs[256];
static int referenced_funcs_count = 0;
static int parse_call(assembly_t *assm, branch br, table *tbl)
{
	info_call *call = br.ptr;

	for (size_t i = 0; i < referenced_funcs_count; i++) {
		if (string_eq(referenced_funcs[i], call->name))
			goto referenced;
	}
	referenced_funcs[referenced_funcs_count++] = call->name;

referenced:;
	op_t comm = { .comment = string_create("CALL") };
	assm_add_op(assm, &comm);
	for (size_t i = 0; i < temp_regs_count; i++) {
		if (temp_regs_taken & (1 << i)) {
			op_t op = { .op = PUSH, .src = temp_regs[i] };
			assm_add_op(assm, &op);
		}
	}
	for (size_t i = 0; i < assm->argc; i++) {
		op_t push = { .op = PUSH, .src = arg_regs[i] };
		assm_add_op(assm, &push);
	}

	enum REG argr[arg_regs_count];
	for (size_t i = 0; i < call->argc; i++) {
		enum REG r = arg_regs[i];
		eval_expr(assm, call->args[i], r, tbl);
	}
	op_t op = { .op = CALL, .label = call->name };
	assm_add_op(assm, &op);

	for (size_t i = assm->argc - 1; i != -1; i--) {
		op_t pop = { .op = POP, .dest = arg_regs[i] };
		assm_add_op(assm, &pop);
	}
	for (size_t i = temp_regs_count - 1; i != -1; i--) {
		if (temp_regs_taken & (1 << i)) {
			op_t op = { .op = POP, .dest = temp_regs[i] };
			assm_add_op(assm, &op);
		}
	}
	return 0;
}


static int parse(assembly_t *assm, branch *brs, size_t *index, table *tbl)
{
	branch br = brs[*index];
	if (br.flags & BRANCH_ISLEAF) {
		if (br.flags & BRANCH_TYPE_C)
			return parse_control_word(assm, brs, index, tbl);
		else if (br.flags & BRANCH_TYPE_VAR)
			return parse_var(assm, br, tbl);
		else if (br.flags & BRANCH_TYPE_CALL)
			return parse_call(assm, br, tbl);
		fprintf(stderr, "Invalid type flags! 0x%x (Please file a bug report)\n", br.flags);
		return -1;
	} else {
		for (size_t i = 0; i < br.len; i++) {
			if (parse(assm, br.ptr, &i, tbl) < 0)
				return -1;
		}
	}
	return 0;
}

#if 0
static int convert_var(assembly_t *assm, info_var *inf)
{
	string strs[8] = { inf->name, string_create(":\t") };
	size_t count = 2;
	if (strcmp(inf->type->buf, "string") == 0) {
		string strs2[4];
		size_t count2 = 0;
		strs[count++] = string_create("db\t\"");
		strs[count++] = string_create("TODO");
		strs[count++] = string_create("\"");
		strs2[count2++] = inf->name;
		strs2[count2++] = string_create("_len:\tequ $ - ");
		strs2[count2++] = inf->name;
		strs2[count2++] = string_create("\n");
		assm_add_op(assm, string_concat(strs, count);
		assm_add_op(assm, string_concat(strs2, count2);
		free(strs2[0]);
		free(strs2[2]);
		goto freestrs;
	} else if (strcmp(inf->type->buf, "sbyte" ) == 0) {
		strs[count++] = string_create("db\t");
		strs[count++] = string_create("TODO");
	} else if (strcmp(inf->type->buf, "sshort") == 0) {
		strs[count++] = string_create("dw\t");
		strs[count++] = string_create("TODO");
	} else if (strcmp(inf->type->buf, "sint"  ) == 0) {
		strs[count++] = string_create("dd\t");
		strs[count++] = string_create("TODO");
	} else if (strcmp(inf->type->buf, "slong" ) == 0) {
		strs[count++] = string_create("dq\t");
		strs[count++] = string_create("TODO");
	} else {
		printf("Unknown type: %s\n", inf->type->buf);
		return -1;
	}
	assm_add_op(assm, string_concat(strs, count);
freestrs:
	for (size_t i = 0; i < count; i++) {
		if (i != 0 && i != 2)
			free(strs[i]);
	}
	return 0;
}
#endif

static int cmp_keys(const void *x, const void *y)
{
	return !string_eq((string)x, (string)y);
}

static int convert_func(assembly_t *assm, info_func *inf, branch root)
{
	if (inf->arg_count > arg_regs_count) {
		fprintf(stderr, "Function '%s' has %lu too many arguments (max: %lu)\n",
		        inf->name->buf, inf->arg_count - (arg_regs_count),
			arg_regs_count);
		return -1;
	}
	table tbl;
	table_init(&tbl, cmp_keys);
	for (char i = 0; i < inf->arg_count; i++) {
		enum REG reg = arg_regs[i];
		if (table_add(&tbl, inf->arg_names[i], (void *)reg) < 0) {
			fprintf(stderr, "Duplicate entry: %s\n", inf->arg_names[i]);
			return -1;
		}
	}
	for (size_t i = 0; i < root.len; i++) {
		if (parse(assm, root.ptr, &i, &tbl) < 0)
			return -1;
	}
	table_free(&tbl);
	return 0;
}


static const char *arg_to_str(arg_t arg, unsigned int bits)
{
	static char buf[256];
	if (arg.is_num) {
		sprintf(buf, "%ld", arg.val);
		return buf;
	} else {
		switch(bits) {
		case BITS_64:
			switch (arg.reg) {
			case AX : return "rax";
			case BX : return "rbx";
			case CX : return "rcx";
			case DX : return "rdx";
			case DI : return "rdi";
			case R8 : return "r8" ;
			case R9 : return "r9" ;
			case R10: return "r10";
			case R11: return "r11";
			case R12: return "r12";
			case R13: return "r13";
			case R14: return "r14";
			case R15: return "r15";
			default:
				sprintf(buf, "x86_64_inval_%u", arg.reg);
				return buf;
			}
		default:
			sprintf(buf, "inval_%u", 2 << (1 << bits));
			return buf;
		}
	}
}


static int print_op(op_t op)
{
	if (op.is_label) {
		printf("%s:", op.label->buf);
	} else if (op.op != 0) {
		printf("\t");
		arg_t s = op.src, d = op.dest;
		unsigned int b = BITS_64; //op.bits;
		switch (op.op) {
		case MOV : printf("mov" ); break;
		case PUSH: printf("push"); break;
		case POP : printf("pop "); break;

		case ADD : printf("add" ); break;
		case SUB : printf("sub" ); break;
		case IMUL: printf("imul"); break;
		case IDIV: printf("idiv"); break;

		case XOR: printf("xor"); break;

		case CMP: printf("cmp"); break;

		case JMP : printf("jmp" ); break;
		case JE  : printf("je"  ); break;
		case JNE : printf("jne" ); break;
		case JL  : printf("jl"  ); break;
		case JG  : printf("jg"  ); break;
		case JLE : printf("jle" ); break;
		case JGE : printf("jge" ); break;
		case CALL: printf("call"); break;
		case RET : printf("ret" ); break;
		default:
			printf("INVAL_OP\t%u", op.op);
		}
		switch(op.op) {
		case MOV:
		case ADD:
		case SUB:
		case XOR:
		case CMP:
		case IMUL:
			printf("\t%s,%s", arg_to_str(d, b), arg_to_str(s, b));
			break;

		case IDIV:
		case PUSH:
			printf("\t%s", arg_to_str(s, b));
			break;

		case POP:
			printf("\t%s", arg_to_str(d, b));
			break;

			//printf("\t%s,%s,%s", arg_to_str(d, b), arg_to_str(s, b), arg_to_str(op.arg2, b));
			//break;

		case CALL:
		case JMP :
		case JE  :
		case JNE :
		case JL  :
		case JG  :
		case JLE :
		case JGE :
			printf("\t%s", op.label->buf);
			break;
		case RET:
			break;
		default:
			printf("\tUNDEFINED");
		}
	}
	string c = op.comment;
	if (c != NULL)
		printf("\t; %s", c->buf);
	printf("\n");
	return 0;
}


static void print_ops(assembly assm)
{
	for (op_t *op = assm->start; op != NULL; op = op->next)
		print_op(*op);
}


int asm_gen()
{
#if 0
	if (global_vars_count > 0) {
		printf("SECTION .data\n");
		for (size_t i = 0; i < global_vars_count; i++) {
			if (convert_var(&assm, &global_vars[i]) < 0)
				return -1;	
		}
	}
#endif
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
	assembly_t assms[256];

	for (size_t i = 0; i < global_funcs_count; i++) {
		assms[i].argc = global_funcs[i].arg_count;
		assms[i].count = 1;
		assms[i].commentcount = 0;
		assms[i].start = calloc(1, sizeof(*assms->start));
		assms[i].start->label = string_copy(global_funcs[i].name);
		assms[i].start->is_label = 1;
		assms[i].end   = assms[i].start;
		if (convert_func(&assms[i], &global_funcs[i], global_func_branches[i]) < 0)
			return -1;
	}

	for (size_t i = 0; i < global_funcs_count; i++)
		printf("global %s\n", global_funcs[i].name->buf);

	FILE *rstdout = stdout;
	stdout = stderr;
	fprintf(stderr, "-- Unoptimized --\n\n");
	for (size_t i = 0; i < global_funcs_count; i++)
		print_ops(&assms[i]);
	stdout = rstdout;
	fprintf(stderr, "\n\n-- Optimized --\n\n");

	for (size_t i = 0; i < global_funcs_count; i++) {
		asm_optimize(&assms[i]);
		print_ops(&assms[i]);
	}

	for (size_t i = 0; i < referenced_funcs_count; i++) // TODO
		printf("extern %s\n", referenced_funcs[i]->buf);

	for (size_t i = 0; i < global_funcs_count; i++) {
		for (op_t *op = assms[i].start; op != NULL; op = op->next) {
			free(op->comment);
			free(op);
		}
	}
	return 0;
}
