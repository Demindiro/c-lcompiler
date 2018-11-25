#include "table.h"
#include <stdlib.h>
#include <string.h>

static size_t get_index(table *t, const void *key)
{
	for (size_t i = 0; i < t->count; i++) {
		if (t->cmp(t->keys[i], key) == 0)
			return i;
	}
	return -1;
}


int table_init(table *t, int (*cmp)(const void *key1, const void *key2))
{
	t->capacity = 16;
	t->keys   = malloc(sizeof(*t->keys) * t->capacity);
	if (t->keys == NULL)
		return -1;
	t->values = malloc(sizeof(*t->values) * t->capacity);
	if (t->values == NULL) {
		free(t->keys);
		return -1;
	}
	t->count = 0;
	t->cmp = cmp;
	return 0;
}

void table_free(table *t)
{
	free(t->keys);
	free(t->values);
}


int table_set(table *t, const void *key, const void *value)
{
	size_t i = get_index(t, key);
	if (i == -1) {
		if (t->count == t->capacity) {
			size_t nc = t->capacity * 3 / 2;
			void *tmp = realloc(t->keys, nc * sizeof(*t->keys));
			if (tmp == NULL)
				return -1;
			t->keys = tmp;
			tmp = realloc(t->values, nc * sizeof(*t->values));
			if (tmp == NULL)
				return -1;
			t->values = tmp;
		}
		i = t->count++;
		t->keys[i] = key;
	}
	t->values[i] = value;
	return 0;
}

const void *table_get(table *t, const void *key)
{
	size_t i = get_index(t, key);
	if (i == -1)
		return NULL;
	return t->values[i];
}

int table_add(table *t, const void *key, const void *value)
{
	size_t i = get_index(t, key);
	if (i != -1)
		return -1;
	i = t->count++;
	t->keys[i] = key;
	t->values[i] = value;
	return 0;
}
