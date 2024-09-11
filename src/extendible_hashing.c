#include "extendible_hashing.h"
#include <inttypes.h>
#include <stdio.h>

struct bucket {
    uint16_t size;
    uint8_t local_depth;
    void* items;
};

struct extendible_hashing_hashtable {
    size_t key_size;
    size_t val_size;
    uint64_t (*hash)(const void*, size_t);
    bool (*is_equal)(const void*, const void*, size_t);

    uint32_t bucket_count;
    uint32_t empty_bucket_count;
    uint16_t bucket_capacity;
    uint8_t global_depth;
    struct bucket** buckets;
    size_t dir_count;
    struct bucket** dirs;
};

eh_hashtable_t*
eh_create(uint16_t bucket_capacity, size_t key_size, size_t val_size, uint64_t (*const hash)(const void*, size_t),
          bool (*const is_equal)(const void*, const void*, size_t)) {
    assert(0 != bucket_capacity && 0 != key_size && 0 != val_size && NULL != hash && NULL != is_equal);

    eh_hashtable_t* table = malloc(sizeof(eh_hashtable_t));
    *table = (eh_hashtable_t){
        .key_size = key_size,
        .val_size = val_size,
        .hash = hash,
        .is_equal = is_equal,

        .bucket_count = 2,
        .empty_bucket_count = 2,
        .bucket_capacity = bucket_capacity,
        .global_depth = 1,
        .buckets = calloc(2, sizeof(struct bucket*)),
        .dir_count = 2,
        .dirs = calloc(2, sizeof(struct bucket*)),
    };

    for (uint32_t i = 0; i < table->bucket_count; ++i) {
        table->buckets[i] = malloc(sizeof(struct bucket));
        *table->buckets[i] = (struct bucket){
            .size = 0,
            .local_depth = table->global_depth,
            .items = calloc(bucket_capacity, key_size + val_size),
        };
        table->dirs[i] = table->buckets[i];
    }
    return table;
}

void
eh_destroy(eh_hashtable_t* table) {
    assert(NULL != table);
    for (uint32_t i = 0; i < table->bucket_count; ++i) {
        free(table->buckets[i]->items);
        free(table->buckets[i]);
    }
    free(table->buckets);
    free(table->dirs);
    free(table);
}

[[nodiscard]] static inline void*
get_key_ptr(const eh_hashtable_t* table, const struct bucket* bucket, uint16_t i) {
    return (char*)bucket->items + (i * table->key_size);
}

[[nodiscard]] static inline void*
get_val_ptr(const eh_hashtable_t* table, const struct bucket* bucket, uint16_t i) {
    return (char*)bucket->items + (table->bucket_capacity * table->key_size) + (i * table->val_size);
}

eh_iterator_t
eh_iter(const eh_hashtable_t* table) {
    assert(NULL != table);
    return (eh_iterator_t){
        .table = (eh_hashtable_t*)table,
        .bucket_index = 0,
        .item_index = 0,
    };
}

bool
eh_next(eh_iterator_t* iterator, void** key, void** val) {
    assert(NULL != iterator && NULL != key && NULL != val);

    if (iterator->bucket_index == iterator->table->bucket_count) {
        return false;
    }
    struct bucket* bucket = iterator->table->buckets[iterator->bucket_index];
    *key = get_key_ptr(iterator->table, bucket, iterator->item_index);
    *val = get_val_ptr(iterator->table, bucket, iterator->item_index);

    if (++iterator->item_index == bucket->size) {
        iterator->item_index = 0;
        ++iterator->bucket_index;
    }
    return true;
}

[[nodiscard]] static struct bucket*
get_bucket_ptr(const eh_hashtable_t* table, const void* key) {
    return table->dirs[table->hash(key, table->key_size) & ((1 << table->global_depth) - 1)];
}

void
eh_insert(eh_hashtable_t* table, const void* key, const void* val) {
    assert(NULL != table && NULL != key && NULL != val);

    struct bucket* bucket = get_bucket_ptr(table, key);
    for (uint16_t i = 0; i < bucket->size; ++i) {
        if (table->is_equal(get_key_ptr(table, bucket, i), key, table->key_size)) {
            memcpy(get_val_ptr(table, bucket, i), val, table->val_size);
            return; // Update
        }
    }

    if (0 == bucket->size) {
        --table->empty_bucket_count;
    }
    memcpy(get_key_ptr(table, bucket, bucket->size), key, table->key_size);
    memcpy(get_val_ptr(table, bucket, bucket->size), val, table->val_size);
    if (++bucket->size < table->bucket_capacity) {
        return; // Insert
    }

    if (bucket->local_depth == table->global_depth) { // Expansion
        printf("Hello World!\n");
        table->dirs = realloc(table->dirs, 2 * table->dir_count * sizeof(struct bucket*));
        printf("Hello World?\n");
        for (size_t i = 0; i < table->dir_count; ++i) {
            table->dirs[i + table->dir_count] = table->dirs[i];
        }
        table->dir_count *= 2;
        ++table->global_depth;
    }

    struct bucket* new_bucket = malloc(sizeof(struct bucket)); // Split:
    *new_bucket = (struct bucket){
        .size = 0,
        .local_depth = 1 + bucket->local_depth,
        .items = calloc(table->bucket_capacity, table->key_size + table->val_size),
    };

    uint64_t high_bit = 1 << bucket->local_depth;
    ++bucket->local_depth;
    for (uint16_t i = 0; i < bucket->size; ++i) {
        void* k = get_key_ptr(table, bucket, i);
        void* v = get_val_ptr(table, bucket, i);
        if (table->hash(k, table->key_size) & high_bit) {
            memcpy(get_key_ptr(table, new_bucket, new_bucket->size), k, table->key_size);
            memcpy(get_val_ptr(table, new_bucket, new_bucket->size), v, table->val_size);
            ++new_bucket->size;
        } else {
            memcpy(get_key_ptr(table, bucket, i - new_bucket->size), k, table->key_size);
            memcpy(get_val_ptr(table, bucket, i - new_bucket->size), v, table->val_size);
        }
    }
    bucket->size -= new_bucket->size;

    for (size_t i = table->hash(key, table->key_size) & (high_bit - 1); i < table->dir_count; i += high_bit) {
        table->dirs[i] = i & high_bit ? new_bucket : bucket;
    }
    if (0 == (table->bucket_count & (table->bucket_count - 1))) { // Is power of 2
        printf("F realloc\n");
        table->buckets = realloc(table->buckets, 2 * table->bucket_count * sizeof(struct bucket*));
        printf("F realloc?\n");
    }
    table->buckets[table->bucket_count] = new_bucket;
    ++table->bucket_count;
}

void*
eh_lookup(const eh_hashtable_t* table, const void* key) {
    assert(NULL != table && NULL != key);

    struct bucket* bucket = get_bucket_ptr(table, key);
    for (uint16_t i = 0; i < bucket->size; ++i) {
        if (table->is_equal(get_key_ptr(table, bucket, i), key, table->key_size)) {
            return get_val_ptr(table, bucket, i);
        }
    }
    return NULL;
}

void
eh_erase(eh_hashtable_t* table, const void* key) {
    assert(NULL != table && NULL != key);

    struct bucket* bucket = get_bucket_ptr(table, key);
    for (uint16_t i = 0; i < bucket->size; ++i) {
        if (table->is_equal(get_key_ptr(table, bucket, i), key, table->key_size)) {
            for (uint16_t j = i; j < bucket->size - 1; ++j) { // Shift
                memcpy(get_key_ptr(table, bucket, j), get_key_ptr(table, bucket, j + 1), table->key_size);
                memcpy(get_val_ptr(table, bucket, j), get_val_ptr(table, bucket, j + 1), table->val_size);
            }
            memset(get_key_ptr(table, bucket, bucket->size - 1), 0, table->key_size);
            memset(get_val_ptr(table, bucket, bucket->size - 1), 0, table->val_size);
            --bucket->size;

            if (bucket->size == 0) {
                printf("%hu %hu\n", table->empty_bucket_count, table->bucket_count);
            }
            if (0 == bucket->size && ++table->empty_bucket_count >= table->bucket_count / 2) { // Rehash
                printf("Rehashing\n");
                eh_hashtable_t* new_table = eh_create(table->bucket_capacity, table->key_size, table->val_size,
                                                      table->hash, table->is_equal);
                for (uint32_t i = 0; i < table->bucket_count; ++i) {
                    struct bucket* bucket = table->buckets[i];
                    for (uint16_t j = 0; j < bucket->size; ++j) {
                        eh_insert(new_table, get_key_ptr(table, bucket, j), get_val_ptr(table, bucket, j));
                    }
                }

                for (uint32_t i = 0; i < table->bucket_count; ++i) {
                    free(table->buckets[i]->items);
                    free(table->buckets[i]);
                }
                free(table->buckets);
                free(table->dirs);
                table->bucket_count = new_table->bucket_count;
                table->empty_bucket_count = new_table->empty_bucket_count;
                table->global_depth = new_table->global_depth;
                table->buckets = new_table->buckets;
                table->dir_count = new_table->dir_count;
                table->dirs = new_table->dirs;
                free(new_table);
            }
        }
    }
}
