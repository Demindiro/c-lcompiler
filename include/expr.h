#ifndef EXPR_H
#define EXPR_H

#include "include/string.h"

#define EXPR_MASK
#define EXPR_ISLEAF 0x100

#define EXPR_OP_MASK 0xFF
#define EXPR_OP_T_MASK 0xF0
// Arithemic
#define EXPR_OP_A      0x00
#define EXPR_OP_A_ADD  0x01 // +
#define EXPR_OP_A_SUB  0x02 // -
#define EXPR_OP_A_MUL  0x03 // *
#define EXPR_OP_A_DIV  0x04 // /
#define EXPR_OP_A_REM  0x05 // %
#define EXPR_OP_A_MOD  0x06 // %%
// Comparison
#define EXPR_OP_C      0x10
#define EXPR_OP_C_EQU  0x11 // ==
#define EXPR_OP_C_NEQ  0x12 // !=
#define EXPR_OP_C_GRT  0x13 // >
#define EXPR_OP_C_LST  0x14 // <
#define EXPR_OP_C_GET  0x15 // >=
#define EXPR_OP_C_LET  0x16 // <=
// Logical
#define EXPR_OP_L      0x20
#define EXPR_OP_L_NOT  0x21 // !
#define EXPR_OP_L_AND  0x22 // &&
#define EXPR_OP_L_OR   0x23 // ||
// Bitwise
#define EXPR_OP_B      0x30
#define EXPR_OP_B_NOT  0x31 // ~
#define EXPR_OP_B_AND  0x32 // &
#define EXPR_OP_B_OR   0x33 // |
#define EXPR_OP_B_XOR  0x34 // ^
#define EXPR_OP_B_LS   0x35 // <<
#define EXPR_OP_B_RS   0x36 // >>

#define EXPR_ISNUM     0x1000

typedef struct expr_branch {
	int flags;
	int len;
	union {
		void *ptr;
		string val;
		struct expr_branch *branches;
	};
} expr_branch;

int expr_parse(string str, expr_branch *root);

const char *debug_expr_op_to_str(int flags);

#ifndef NDEBUG
void debug_expr(expr_branch br, int lvl);
#else
#define debug_expr(br, lvl) ""
#endif

#endif
