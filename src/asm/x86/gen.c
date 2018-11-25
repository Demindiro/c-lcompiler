#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/string.h"
#include "expr.h"
#include "table.h"
#include "code/read.h"
#include "code/branch.h"
#include "util.h"


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
} op_t;


typedef struct assembly {
	size_t size;
	size_t count;
	op_t   *lines;
	string *comments;
} assembly_t;


const char *var_types[] = {
	"bool",
	"byte",
	"short",
	"int",
	"long",
	"ubyte",
	"ushort",
	"uint",
	"ulong",
	"void",
};


enum REG  arg_regs[] = { DI, SI, DX, CX, R8, R9 };
enum REG temp_regs[] = { R8, R9, R10, R11, R12, R13, R14, R15 }; 
unsigned int temp_regs_taken;


static string str_tab;
static string str_comma;
static string str_op_mov;
static string str_op_add;
static string str_op_sub;
static string str_op_imul;
static string str_op_idiv;
static string str_op_push;
static string str_op_pop;
static string str_reg_rax;
static string str_reg_rbx;
static string str_reg_rcx;
static string str_reg_rdx;

__attribute__((constructor))
void init_strs()
{
	str_tab     = string_create("\t");
	str_comma   = string_create("," );

	str_op_mov  = string_create("mov" );
	str_op_add  = string_create("add" );
	str_op_sub  = string_create("sub" );
	str_op_imul = string_create("imum");
	str_op_idiv = string_create("idiv");
	str_op_push = string_create("push");
	str_op_pop  = string_create("pop" );

	str_reg_rax = string_create("rax");
	str_reg_rbx = string_create("rbx");
	str_reg_rcx = string_create("rcx");
	str_reg_rdx = string_create("rdx");

	const char *arg_regs_a[] = {
		"rax", "rbx", "rcx", "rdx",
	};
	const char *temp_regs_a[] = {
		"r8" , "r9" , "r10", "r11",
		"r12", "r13", "r14", "r15",
	};
}


static int parse(assembly_t *assm, branch *brs, size_t *index, table *tbl);


static enum REG get_temp_reg_once()
{
	for (size_t i = 0; i < sizeof(temp_regs) / sizeof(*temp_regs); i++) {
		if (!(temp_regs_taken & (2 << i)))
			return temp_regs[i];
	}
	return -1;
}

static enum REG get_temp_reg()
{
	for (size_t i = 0; i < sizeof(temp_regs) / sizeof(*temp_regs); i++) {
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
	for (size_t i = 0; i < sizeof(temp_regs) / sizeof(*temp_regs); i++) {
		if (temp_regs[i] == reg) {
			temp_regs_taken &= ~(1 << i);
			return;
		}
	}
}

static size_t is_var_type(char *str)
{
	for (size_t i = 0; i < sizeof(var_types) / sizeof(*var_types); i++)
		if (strcmp(str, var_types[i]) == 0)
			return i;
	return -1;
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
		op_t push = { .op = PUSH, .src  = DX };
		op_t zero = { .op = XOR , .dest = DX, .src = DX };
		assm->lines[assm->count++] = push;
		assm->lines[assm->count++] = zero;
		if (flags & EXPR_ISNUM) {
			src = get_temp_reg_once();
			op_t mov = { .op = MOV, .dest = src, .src = { .val = val, .is_num = 1 }};
			assm->lines[assm->count++] = mov;
		}
		op_t div  = { .op = op  , .src  = src };
		//op_t mov  = { .op = MOV , .dest = dest, .src = src };
		op_t pop  = { .op = POP , .dest = DX };
		assm->lines[assm->count++] = div;
		//assm->lines[assm->count++] = mov;
		assm->lines[assm->count++] = pop;
	} else if (op == IMUL) {
		op_t o = { .op = op, .dest = dest, .src = dest, .arg2 = src };
		assm->lines[assm->count++] = o;
	} else {
		op_t o = { .op = op, .dest = dest };
		if (flags & EXPR_ISNUM) {
			o.src.val = val;
			o.src.is_num = 1;
		} else {
			o.src.reg = src;
		}
		assm->lines[assm->count++] = o;
	}
	return 0;
}


static int _eval_expr(assembly_t *assm, enum REG dreg, expr_branch root, table *tbl)
{
	const char *ar;
	if (root.flags & EXPR_ISLEAF) {
		char buf[256];
		sprintf(buf, "EXPR %s %s", debug_expr_op_to_str(root.flags), root.val->buf);
		assm->comments[assm->count] = string_create(buf);
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
	} else {
		assm->comments[assm->count++] = string_create("EXPR_START");
		enum REG treg = get_temp_reg();
		op_t op = { .op = PUSH, .src = AX };
		assm->lines[assm->count++] = op;
		for (size_t i = 0; i < root.len; i++) {
			if (_eval_expr(assm, dreg, root.branches[i], tbl) < 0)
				return -1;
		}
		if (parse_op(assm, root.flags, dreg, treg, -1) < 0)
			return -1;
		free_temp_reg(treg);
		op.op   = POP;
		op.dest = op.src;
		assm->lines[assm->count++] = op;
		assm->comments[assm->count++] = string_create("EXPR_END");
	}
	return 0;
}


static int eval_expr(assembly_t *assm, expr_branch root, enum REG dreg, table *tbl)
{
	if (root.flags & EXPR_ISLEAF) {
		string s[] = { string_create("OP "), root.val };
		assm->comments[assm->count] = string_concat(s, 2);
		free(s[0]);
		op_t op = { .op = MOV, .dest = dreg };
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
		assm->lines[assm->count++] = op;
	// Tip: if it is not a leaf, it has at least 2 branches
	} else {

		int offset, expr2 = 1;
		op_t op = { .dest = dreg }, tmp;// = { .dest = dreg };
		if (dreg != AX) {
			op_t mov = { .op = PUSH, .src = AX };
			tmp = mov;
			assm->lines[assm->count++] = tmp;
		}

		expr_branch br0 = root.branches[0], br1 = root.branches[1];
		switch (br1.flags & EXPR_OP_MASK) {
		case EXPR_OP_A_ADD: op.op = ADD; break;
		case EXPR_OP_A_SUB: op.op = SUB; break;
		default:
			offset = 0;
			expr2  = 0;
			goto default_parse;
		}
		assm->comments[assm->count++] = string_create("EXPR2_START");
		if (br0.flags & EXPR_ISLEAF && br0.flags & EXPR_ISNUM) {
			eval_expr(assm, br1, AX, tbl);
			op.src.is_num = 1;
			op.src.val = atoi(br0.val->buf);
		} else if (br1.flags & EXPR_ISLEAF && br1.flags & EXPR_ISNUM) {
			eval_expr(assm, br0, AX, tbl);
			op.src.is_num = 1;
			op.src.val = atoi(br1.val->buf);
		} else {
			eval_expr(assm, br0, AX, tbl);
			op.src.reg = get_temp_reg();
			op_t o = { .op = MOV, .dest = op.src, .src = AX };
			assm->lines[assm->count++] = o;
			eval_expr(assm, br1, AX, tbl);
			free_temp_reg(op.src.reg);
		}
		op_t mov = { .op = MOV, .dest = dreg, .src = AX };
		assm->lines[assm->count++] = mov;
		assm->lines[assm->count++] = op;
		offset = 2;
	default_parse:
		for (size_t i = offset; i < root.len; i++) { 
			if (_eval_expr(assm, dreg, root.branches[i], tbl) < 0)
				return -1;
		}

		if (dreg != AX) {
			if (!expr2) {
				op_t mov = { .op = MOV, .dest = dreg, .src = AX };
				assm->lines[assm->count++] = mov;
				free_temp_reg(tmp.dest.reg);
			}
			op_t pop = { .op = POP, .dest = AX };
			assm->lines[assm->count++] = pop;
		}
		if (expr2)
			assm->comments[assm->count++] = string_create("EXPR2_END");
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
	assm->lines[assm->count++] = op;
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
		assm->lines[assm->count++] = cmp;
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
		assm->comments[assm->count++] = string_create("IF");
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
			assm->lines[assm->count++] = jmp;
			assm->lines[assm->count++] = l;
			lbl = lblelse;
			(*index) += 2;
			parse(assm, brs, index, tbl);
		}
		op_t l = { .label = lbl, .is_label = 1 };
		assm->lines[assm->count++] = l;
	} else if (br.flags & BRANCH_CTYPE_RET) {
		assm->comments[assm->count++] = string_create("RET");
		eval_expr(assm, *br.expr, AX, tbl);
		op_t op = { .op = RET };
		assm->lines[assm->count++] = op;
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
	assm->comments[assm->count++] = string_concat(s, 4);
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
referenced:

	assm->comments[assm->count++] = string_create("CALL");
	for (size_t i = 0; i < sizeof(temp_regs) / sizeof(*temp_regs); i++) {
		if (temp_regs_taken & (1 << i)) {
			op_t op = { .op = PUSH, .src = temp_regs[i] };
			assm->lines[assm->count++] = op;
		}
	}
	enum REG argr[sizeof(arg_regs) / sizeof(*arg_regs)];
	for (size_t i = 0; i < call->argc; i++) {
		enum REG r = arg_regs[i];
		if (1) { // TODO
			op_t push = { .op = PUSH, .src = r };
			assm->lines[assm->count++] = push;
		}
		eval_expr(assm, call->args[i], r, tbl);
	}

	op_t op = { .op = CALL, .label = call->name };
	assm->lines[assm->count++] = op;

	for (size_t i = call->argc - 1; i != -1; i--) {
		op_t pop = { .op = POP, .dest = arg_regs[i] };
		assm->lines[assm->count++] = pop;
	}
	for (size_t i = sizeof(temp_regs) / sizeof(*temp_regs) - 1; i != -1; i--) {
		if (temp_regs_taken & (1 << i)) {
			op_t op = { .op = POP, .dest = temp_regs[i] };
			assm->lines[assm->count++] = op;
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
		assm->lines[assm->count++] = string_concat(strs, count);
		assm->lines[assm->count++] = string_concat(strs2, count2);
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
	assm->lines[assm->count++] = string_concat(strs, count);
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
	if (inf->arg_count > sizeof(arg_regs) / sizeof(*arg_regs)) {
		fprintf(stderr, "Function '%s' has %lu too many arguments (max: %lu)\n",
		        inf->name->buf, inf->arg_count - (sizeof(arg_regs) / sizeof(*arg_regs)),
			sizeof(arg_regs) / sizeof(*arg_regs));
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
	op_t func = { .label = inf->name, .is_label = 1 };
	assm->lines[assm->count++] = func;
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


static int print_op(assembly_t *assm, size_t i)
{
	op_t op = assm->lines[i];
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
			printf("\t%s,%s", arg_to_str(d, b), arg_to_str(s, b));
			break;

		case IDIV:
		case PUSH:
			printf("\t%s", arg_to_str(s, b));
			break;

		case POP:
			printf("\t%s", arg_to_str(d, b));
			break;

		case IMUL:
			printf("\t%s,%s,%s", arg_to_str(d, b), arg_to_str(s, b), arg_to_str(op.arg2, b));
			break;

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
	string c = assm->comments[i];
	if (c != NULL)
		printf("\t; %s", c->buf);
	printf("\n");
	return 0;
}


int asm_gen()
{
	assembly_t assm = { .size = 65536, .count = 0 };
	assm.lines = calloc(assm.size, sizeof(*assm.lines));
	assm.comments = calloc(assm.size, sizeof(*assm.comments));

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
	for (size_t i = 0; i < global_funcs_count; i++) {
		if (convert_func(&assm, &global_funcs[i], global_func_branches[i]) < 0)
			return -1;
	}
	for (size_t i = 0; i < global_funcs_count; i++)
		printf("global %s\n", global_funcs[i].name->buf);
	for (size_t i = 0; i < referenced_funcs_count; i++) // TODO
		printf("extern %s\n", referenced_funcs[i]->buf);
	for (size_t i = 0; i < assm.count; i++)
		print_op(&assm, i);
	free(assm.lines);
	free(assm.comments);
	return 0;
}
