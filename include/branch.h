#ifndef BRANCH_H
#define BRANCH_H

// I despise header files
//#include "info.h"
#include "expr.h"

#define BRANCH_MASK        0xF
#define BRANCH_ISLEAF      0x1
#define BRANCH_HASCTRLEXPR 0x2

#define BRANCH_CTYPE_MASK  0xFF00
#define BRANCH_CTYPE_WHILE 0x0100
#define BRANCH_CTYPE_FOR   0x0200
#define BRANCH_CTYPE_IF    0x0400
#define BRANCH_CTYPE_ELSE  0x0800
#define BRANCH_CTYPE_RET   0x1000

#define BRANCH_TYPE_MASK   0xF0
#define BRANCH_TYPE_VAR    0x10
#define BRANCH_TYPE_C      0x20
#define BRANCH_TYPE_CALL   0x40


typedef struct branch {
	int flags;
	int len;
	union {
		void *ptr;
		char *str;
		struct branch *br; /* DEPRECATED */
		struct branch *branches;
		struct expr_branch *expr;
	};
} branch;

#ifndef NDEBUG
void debug_branch(branch br);
#else
#define debug_branch(br) ""
#endif

#endif
