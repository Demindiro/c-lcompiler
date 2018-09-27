#ifndef BRANCH_H
#define BRANCH_H

#define BRANCH_MASK        0xF00
#define BRANCH_ISLEAF      0x100
#define BRANCH_HASCTRLEXPR 0x200

#define BRANCH_CTYPE_MASK  0xF
#define BRANCH_CTYPE_WHILE 0x1
#define BRANCH_CTYPE_FOR   0x2
#define BRANCH_CTYPE_IF    0x4
#define BRANCH_CTYPE_RET   0x8

#define BRANCH_TYPE_MASK 0xF0
#define BRANCH_TYPE_VAR  0x10
#define BRANCH_TYPE_C    0x20

typedef struct branch {
	int flags;
	int len;
	void *ptr;
} branch;

#endif
