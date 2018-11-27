#include "x86.h"
#include <stdlib.h>


enum REG  arg_regs[] = { DI, SI, DX, CX, R8, R9 };
enum REG temp_regs[] = { R10, R11, R12, R13, R14, R15 }; 
unsigned int temp_regs_taken;
size_t  arg_regs_count = sizeof(arg_regs ) / sizeof(*arg_regs );
size_t temp_regs_count = sizeof(temp_regs) / sizeof(*temp_regs);


int assm_add_op(assembly assm, op_t *op)
{
	static int counter = 0;
	op_t *o = malloc(sizeof(*o));
	if (o == NULL)
		return -1;
	*o = *op;
	assm->end->next = o;
	o->prev = assm->end;
	assm->end = o;
	if (op->op == 0)
		assm->commentcount++;
	assm->count++;
	return 0;
}


op_t *assm_get_op(assembly assm, size_t i)
{
	if (i >= assm->count)
		return NULL;
	op_t *op;
	if (i * 2 > assm->count) {
		op = assm->end;
		for (size_t k = assm->count - 1; k != i; k--)
			op = op->prev;
	} else {
		op = assm->start;
		for (size_t k = 0; k != i; k++)
			op = op->next;
	}
	return op;
}


int assm_remove_op(assembly assm, op_t *op)
{
	if (op == NULL)
		return -1;
	op_t *prev = op->prev,
	     *next = op->next;
	prev->next = next;
	next->prev = prev;
	if (op->op == 0)
		assm->commentcount--;
	assm->count--;
	free(op->comment);
	free(op);
	return 0;
}


int assm_insert_op(assembly assm, size_t i, op_t *op)
{
	op_t *next = assm_get_op(assm, i);
	if (next == NULL)
		return -1;
	op_t *prev = next->prev;

	op_t *o = malloc(sizeof(*op));
	if (op == NULL)
		return -1;
	*o = *op;

	o->prev = prev;
	if (prev != NULL)
		prev->next = o;
	o->next = next;
	next->prev = o;

	if (op->op == 0)
		assm->commentcount++;
	assm->count++;
	return 0;
}
