#include "code/split.h"
#include <stdlib.h>
#include <string.h>
#include "info.h"


static string copy_line(const char *a, size_t len)
{
	size_t offset = 0;
	string str = malloc(sizeof(str->len) + len + 1);
	
	str->buf[0] = a[0];
	for (size_t i = 1; i < len; i++) {
		if (strchr("=+-*/%<>|&^ \t\n()[]{},.;", a[i]) != NULL) {
			if (strchr(" \t\n", a[i-1]) != NULL)
				offset++;
		}
		str->buf[i - offset] = a[i];
	}
	len -= offset;

	a = str->buf;
	offset = 0;
	int next_offset_0 = 0, next_offset_1 = 0;
	for (size_t i = 1; i < len - 1; i++) {
		if (strchr("=+-*/%<>|&^ \t\n()[]{},.;", a[i]) != NULL)
			next_offset_1 = strchr(" \t\n", a[i+1]) != NULL;
		str->buf[i - offset] = a[i];
		if (next_offset_0)
			offset++;
		next_offset_0 = next_offset_1;
		next_offset_1 = 0;
	}
	str->buf[len - offset - 1] = a[len - 1];
	len -= offset;

	for (size_t i = 0; i < len; i++)
		str->buf[i] = strchr("\t\n", a[i]) == NULL ? a[i] : ' ';

	str->buf[len] = 0;
	str->len = len;
	return realloc(str, sizeof(str->len) + str->len + 1);
}


lines_t split_func(info_func func)
{
	size_t size = 1024;
	lines_t lines = { .count = 0 };
	lines.lines = malloc(sizeof(*lines.lines) * size);

	char *ptr = func.body;
	while (1) {
		while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == ';')
			ptr++;
		if (*ptr == 0)
			break;
		const char *start = ptr;
		while (1) {
			if (*ptr == '\n' || *ptr == ';' || *ptr == 0) {
				char *p = ptr;
				while (*p == ' ' || *p == '\t' || *p == ';' || *p == '\n')
					p--;
				line_t l = { .row = lines.count + 1, .col = 0 };
				l.str = copy_line(start, p - start + 1);
				lines.lines[lines.count] = l;
				lines.count++;
				ptr++;
				start = ptr;
				if (*ptr == 0)
					goto done;
				break;
			}
			ptr++;
		}
	}

done:
	lines.lines = realloc(lines.lines, sizeof(*lines.lines) * lines.count);
	return lines;
}


lines_t *code_split(info_func *funcs, size_t count)
{
	lines_t *l = malloc(sizeof(*l) * count);
	for (size_t i = 0; i < count; i++) {
		l[i] = split_func(funcs[i]);
	}
	return l;
}
