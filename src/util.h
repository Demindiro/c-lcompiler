#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

#define IS_WHITE(c)    (c == ' ' || c == '\t' || c == '\n')
#define IS_OPERATOR(c) (strchr("=<>""+-""*/%""&|^", c) != NULL)
#define IS_VAR_CHAR(c) (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_')
#define SKIP_WHITE(ptr) while (IS_WHITE(*ptr)) (ptr)++
#define COPY_UNTIL_WHITE(dest, ptr) { char *d = dest; while (!IS_WHITE(*ptr) && *ptr != 0) *(d++) = *(ptr++); *d = 0; } ""
#define COPY_UNTIL_WHITE_OR_CHAR(dest, ptr, c) { char *d = dest; while(!IS_WHITE(*ptr) && *ptr != c && *ptr != 0) *(d++) = *(ptr++); *d = 0; } ""
#define SWAP(a, b) { auto tmp = a; a = b; b = tmp; }

#define debug(msg, ...) fprintf(stderr, msg "\n", ##__VA_ARGS__)

#endif
