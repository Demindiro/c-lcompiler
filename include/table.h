#ifndef TABLE_H
#define TABLE_H

#include <stddef.h>

typedef struct table {
	void *keys;
	void *values;
	size_t capacity;
	size_t count;
	size_t keylen;
	size_t vallen;
	int (*cmp)(const void *key1, const void *key2);
} table;

int table_init(table *t, size_t keylen, size_t vallen, int (*cmp)(const void *key1, const void *key2));

void table_free(table *t);

int table_set(table *t, const void *key, const void *value);

int table_get(table *t, const void *key, void *value);

int table_add(table *t, const void *key, const void *value);

#endif
