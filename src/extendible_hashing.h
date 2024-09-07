#ifndef EXTENDIBLE_HASHING_H
#define EXTENDIBLE_HASHING_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct extendible_hashing_hashtable;
typedef struct extendible_hashing_hashtable eh_hashtable_t;

eh_hashtable_t* eh_create(uint32_t bucket_capacity, size_t key_size, size_t value_size,
                          uint64_t (*const hash)(const void*, size_t),
                          bool (*const is_equal)(const void*, const void*, size_t));
void eh_destroy(eh_hashtable_t* table);

#endif // EXTENDIBLE_HASHING_H
