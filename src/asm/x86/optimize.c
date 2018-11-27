#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "optimize.h"
#include "x86.h"


static int has_src_arg(enum OP op)
{
	if (op == 0)
		return 0;
	switch (op) {
	case POP:
		return 0;
	case PUSH:
	default:
		break;
	}
	return 1;
}


static int has_dest_arg(enum OP op)
{
	if (op == 0)
		return 0;
	switch(op) {
	case PUSH:
		return 0;
	case POP:
	default:
		break;
	}
	return 1;
}


static int is_reg_in_array(enum REG *regs, size_t size, enum REG reg)
{
	for (size_t i = 0; i < size; i++)
		if (regs[i] == reg)
			return 1;
	return 0;
}


static op_t *remove_both(op_t *org, op_t *op, assembly assm)
{
	op_t *prev = org->prev;
	assm_remove_op(assm, org);
	assm_remove_op(assm, op);
	return prev->prev;
}


static void optimize_mov(assembly assm)
{
	op_t *op = assm->start;
	while (op != NULL) {
		if (op->op != MOV)
			goto next;

		if (!op->src.is_num && op->src.reg == op->dest.reg)
			goto remove;

		op_t *next = op->next;
		if (next != NULL && next->op == MOV) {
			if (op->dest.reg == next->dest.reg)
				goto remove;

			op_t *used_in = NULL;
			for (op_t *o = next; o != NULL && !o->is_label; o = o->next) {
				if (has_src_arg(o->op) && o->src.reg == op->dest.reg) {
					if (used_in != NULL)
						goto next;
					used_in = o;
				}
				if ((o->op == MOV || o->op == POP) && (!o->src.is_num && o->src.reg == op->dest.reg)) {
					if (used_in != NULL)
						used_in->src = op->src;
					goto remove;
				}
			}
		}

	next:
		op = op->next;
		continue;
	remove:
		op = op->prev;
		assm_remove_op(assm, op->next);
	}
}


static void optimize_push(assembly assm)
{
	enum REG used_regs[256] = { 0 };
	size_t count = 0;

	op_t *org = NULL;
	op_t *op = assm->start;
	while (op != NULL) {
		if (op->op == 0)
			goto next;

		if (has_src_arg(op->op) && op->src.is_num && !is_reg_in_array(used_regs, count, op->src.reg))
			used_regs[count++] = op->src.reg;
		if (has_src_arg(op->op) && !is_reg_in_array(used_regs, count, op->dest.reg))
			used_regs[count++] = op->dest.reg;

		if (op->op == PUSH && !is_reg_in_array(used_regs, count, op->src.reg)) {
			op_t *pop = op->next;
			while (pop->op != POP)
				pop = pop->next;
			op = op->prev;
			assm_remove_op(assm, op->next);
			assm_remove_op(assm, pop);
		}

#if 0
		if (op->op == POP && op->dest.reg == org->src.reg)
			return remove_both(org, op, assm);
		if (op->op == PUSH) {
			op = optimize_push(op, assm);
			if (op == NULL)
				return NULL;
		}
		if ((op->is_label || op->op == RET || op->op == CALL) ||
		    (has_src_arg(op->op) && !op->src.is_num && op->src.reg == org->src.reg) ||
		    (has_dest_arg(op->op) && op->dest.reg == org->src.reg))
			return op;
#endif
	next:
		op = op->next;
	}
}


static void optimize_pop(assembly assm)
{
	return;
	op_t *op = assm->start;
	op_t *org = op;
	op = op->next;
	op_t *to_remove = NULL;
	while (op != NULL) {
		if (op->op != POP)
			goto next;
		if (op->op == MOV && op->dest.reg == org->dest.reg) {
			while (op->op != PUSH && op->src.reg != org->dest.reg)
				op = op->next;
			remove_both(org, op, assm);
		}
#if 0
		if (op->op == PUSH && op->dest.reg == org->src.reg) {
			if (to_remove != NULL) {
				op_t *prev = org->prev;
				assm_remove_op(assm, org);
				assm_remove_op(assm, to_remove);
				return prev->next;
			}
			to_remove = op;
		}
#endif
		if ((op->is_label || op->op == RET || op->op == CALL) ||
		    (has_src_arg(op->op) && !op->src.is_num && op->src.reg == org->dest.reg) ||
		    (has_dest_arg(op->op) && op->dest.reg == org->dest.reg))
			return;
	next:
		op = op->next;
	}
}


int asm_optimize(assembly assm)
{
	size_t comment_only = 0;
	fprintf(stderr, "Now: %lu...\n", assm->count - assm->commentcount);

	optimize_mov (assm);
	optimize_push(assm);
	optimize_pop (assm);

	fprintf(stderr, "After: %lu\n\n", assm->count - assm->commentcount);
	return 0;
}
