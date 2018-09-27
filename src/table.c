#include "table.h"
#include <stdlib.h>
#include <string.h>

static size_t get_index(table *t, const void *key)
{
	for (size_t i = 0; i < t->count; i++) {
		void *k = ((char *)t->keys) + (i * t->keylen);
		if (t->cmp(k, key) == 0)
			return i;
	}
	return -1;
}


int table_init(table *t, size_t keylen, size_t vallen, int (*cmp)(const void *key1, const void *key2))
{
	t->capacity = 16;
	t->keylen = keylen;
	t->vallen = vallen;
	t->keys   = malloc(keylen * t->capacity);
	if (t->keys == NULL)
		return -1;
	t->values = malloc(vallen * t->capacity);
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
			void *tmp = realloc(t->keys, nc * t->keylen);
			if (tmp == NULL)
				return -1;
			t->keys = tmp;
			tmp = realloc(t->values, nc * t->vallen);
			if (tmp == NULL)
				return -1;
			t->values = tmp;
		}
		i = t->count++;
		memcpy(((char *)t->keys) + (i * t->keylen), key, t->keylen);
	}
	memcpy(((char *)t->values) + (i * t->vallen), value, t->vallen); 
	return 0;
}

int table_get(table *t, const void *key, void *value)
{
	size_t i = get_index(t, key);
	if (i == -1)
		return -1;
	memcpy(value, ((char *)t->values) + (i * t->vallen), t->vallen);
	return 0;
}

int table_add(table *t, const void *key, const void *value)
{
	size_t i = get_index(t, key);
	if (i != -1)
		return -1;
	i = t->count++;
	memcpy(((char *)t->keys  ) + (i * t->keylen), key  , t->keylen);
	memcpy(((char *)t->values) + (i * t->vallen), value, t->vallen);
}
