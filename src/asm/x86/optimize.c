#include <stdlib.h>
#include <stdio.h>
#include "optimize.h"
#include "x86.h"


static int has_src_arg(enum OP op)
{
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
	switch(op) {
	case PUSH:
		return 0;
	case POP:
	default:
		break;
	}
	return 1;
}


static op_t *optimize_noop(op_t *op, assembly assm)
{
	if (op->op == MOV && !op->src.is_num && op->src.reg == op->dest.reg) {
		op_t *prev = op->prev;
		op->prev = op->next;
		op->next = prev;
		free(op->comment);
		free(op);
		assm->count--;
		return prev;
	}
	return op;
}


static op_t *optimize_stack(op_t *op, assembly assm)
{
	if (op->op != PUSH && op->op != POP)
		return op;

	enum OP  rev_op = op->op == PUSH ? POP : PUSH;
	enum REG reg    = has_src_arg(op->op) ? op->src.reg : op->dest.reg;

	op_t *org  = op;
	op = op->next;
	while (op != NULL) {
		if (op->op == 0)
			goto next;
		if (op->op == rev_op && op->src.reg == reg) {
			op_t *prev = org->prev;
			assm_remove_op(assm, org);
			assm_remove_op(assm, op);
			return prev->prev;
		}
		if (op->op == PUSH || op->op == POP) {
			op = optimize_stack(op, assm);
			if (op == NULL)
				return NULL;
		}
		if ((op->is_label) ||
		    (has_src_arg(op->op) && !op->src.is_num && op->src.reg == reg) ||
		    (has_dest_arg(op->op) && op->dest.reg == reg) ||
		    (op->op == RET))
			return op;
	next:
		op = op->next;
	}
	return NULL;
}


int asm_optimize(assembly assm)
{
	fprintf(stderr, "Now: %lu...\n", assm->count);

	op_t *op = assm->start;
	while (op != NULL)
		op = optimize_noop(op, assm)->next;

	op = assm->start;
	while (op != NULL) {
		op = optimize_stack(op, assm);
		if (op == NULL)
			break;
		op = op->next;
	}

	fprintf(stderr, "After: %lu\n\n", assm->count);
	return 0;
}
