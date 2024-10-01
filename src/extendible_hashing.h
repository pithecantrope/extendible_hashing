#ifndef EXTENDIBLE_HASHING_H
#define EXTENDIBLE_HASHING_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct extendible_hashing_hashtable;
typedef struct extendible_hashing_hashtable eh_hashtable_t;

eh_hashtable_t* eh_create(size_t key_size, size_t value_size, unsigned int bucket_capacity,
                          size_t (*const hash)(const void*), int (*const cmp)(const void*, const void*));
void eh_destroy(eh_hashtable_t* table);
void eh_insert(eh_hashtable_t* table, const void* key, const void* value);
void eh_erase(eh_hashtable_t* table, const void* key);
void* eh_lookup(const eh_hashtable_t* table, const void* key);

typedef struct {
    const eh_hashtable_t* table;
    size_t bucket_index;
    unsigned int item_index;
} eh_iterator_t;

static inline eh_iterator_t
eh_iter(const eh_hashtable_t* table) {
    assert(table != NULL);
    return (eh_iterator_t){.table = table};
}

int eh_next(eh_iterator_t* iterator, const void** key, const void** value);

#endif // EXTENDIBLE_HASHING_H
