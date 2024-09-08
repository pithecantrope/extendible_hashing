#ifndef EXTENDIBLE_HASHING_H
#define EXTENDIBLE_HASHING_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct extendible_hashing_hashtable;
typedef struct extendible_hashing_hashtable eh_hashtable_t;
struct extendible_hashing_iterator;
typedef struct extendible_hashing_iterator eh_iterator_t;

[[nodiscard]] eh_hashtable_t* eh_create(uint16_t bucket_capacity, size_t key_size, size_t value_size,
                                        uint64_t (*const hash)(const void*, size_t),
                                        bool (*const is_equal)(const void*, const void*, size_t));
void eh_destroy(eh_hashtable_t* table);
[[nodiscard]] eh_iterator_t eh_iter(eh_hashtable_t* table);
[[nodiscard]] bool eh_next(eh_iterator_t* iterator, void** key, void** val);

#endif // EXTENDIBLE_HASHING_H
