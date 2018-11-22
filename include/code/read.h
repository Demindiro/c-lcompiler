#ifndef CODE_READ_H
#define CODE_READ_H

#include <stddef.h>
#include "../info.h"


size_t     global_vars_count;
size_t     global_funcs_count;
size_t     global_vars_size;
size_t     global_funcs_size;
info_var  *global_vars;
info_func *global_funcs;


int code_parse(char **pptr, char *end, info *inf);

int code_read(char *file);

#endif
