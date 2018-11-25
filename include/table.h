#ifndef TABLE_H
#define TABLE_H

#include <stddef.h>

typedef struct table {
	const void **keys;
	const void **values;
	size_t capacity;
	size_t count;
	int (*cmp)(const void *key1, const void *key2);
} table;

int table_init(table *t, int (*cmp)(const void *key1, const void *key2));

void table_free(table *t);

int table_set(table *t, const void *key, const void *value);

const void *table_get(table *t, const void *key);

int table_add(table *t, const void *key, const void *value);

#endif
