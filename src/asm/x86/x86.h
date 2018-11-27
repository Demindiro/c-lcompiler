#ifndef X86_H
#define X86_H

#include <stddef.h>
#include "include/string.h"

enum OP {
	/* Arithemic */	
	ADD = 100,
	SUB,
	IMUL,
	IDIV,
	
	/* Bitswise */
	AND = 1000,
	OR,
	XOR,
	CMP,
	
	/* Jumps */
	JMP = 10000,
	JE,
	JNE,
	JG,
	JL,
	JLE,
	JGE,
	CALL,
	RET,

	/* Store/load */
	MOV = 100000,
	PUSH,
	POP,
};


enum REG {
	AX = 1, BX, CX, DX,
	AL, AH, BL, BH,
	CL, CH, DL, DH,
	SI, DI, SP, BP,
	IP,
	CS, DS, SS, ES,
	FS, GS,
	R8 , R9 , R10, R11,
	R12, R13, R14, R15,
};


#define BITS_8  0
#define BITS_16 1
#define BITS_32 2
#define BITS_64 3


typedef struct arg {
	union {
		enum REG reg;
		size_t val;
	};
	unsigned int is_num : 1;
} arg_t;


typedef struct op {
	enum OP op;
	union {
		string label;
		//arg_t arg0;
		arg_t src;
	};
	unsigned int is_label : 1;
	unsigned int bits : 2;
	union {
		//arg_t arg1;
		arg_t dest;
	};
	arg_t arg2;
	struct op *prev;
	struct op *next;
	string comment;
} op_t;


typedef struct assembly {
	size_t size;
	size_t count;
	op_t *start;
	op_t *end;
	size_t argc;
	size_t commentcount;
} assembly_t, *assembly;


extern enum REG  arg_regs[];
extern enum REG temp_regs[];
extern unsigned int temp_regs_taken;
extern size_t  arg_regs_count;
extern size_t temp_regs_count;


int assm_add_op(assembly assm, op_t *op);

op_t *assm_get_op(assembly assm, size_t i);

int assm_remove_op(assembly assm, op_t *op);

int assm_insert_op(assembly assm, size_t i, op_t *op);

#define ASSM_ADD_OP(assm, op) { op_t o = op; assm_add_op(assm, &o); }


#endif
