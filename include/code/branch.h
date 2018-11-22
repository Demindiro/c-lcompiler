#ifndef CODE_BRANCH_H
#define CODE_BRANCH_H

#include "../branch.h"

branch *global_func_branches;

int code_branch();

#ifndef NDEBUG
void debug_branch(branch br);
#else
#define debug_branch(br) ""
#endif

#endif
