#include "extendible_hashing.h"

struct bucket {
    uint32_t size;
    uint8_t local_depth;
    void* items;
};

struct extendible_hashing_hashtable {
    uint32_t bucket_capacity;
    size_t key_size;
    size_t value_size;
    uint64_t (*hash)(const void*, size_t);
    bool (*is_equal)(const void*, const void*, size_t);

    struct bucket** buckets;
    uint8_t global_depth;
    size_t dir_count;
    struct bucket** dirs;
};

eh_hashtable_t*
eh_create(uint32_t bucket_capacity, size_t key_size, size_t value_size, uint64_t (*const hash)(const void*, size_t),
          bool (*const is_equal)(const void*, const void*, size_t)) {
    if (0 == bucket_capacity || 0 == key_size || 0 == value_size || NULL == hash || NULL == is_equal) {
        return NULL;
    }

    struct extendible_hashing_hashtable* table = malloc(sizeof(struct extendible_hashing_hashtable));
    if (NULL == table) {
        goto free_table;
    }
    *table = (struct extendible_hashing_hashtable){
        .bucket_capacity = bucket_capacity,
        .key_size = key_size,
        .value_size = value_size,
        .hash = hash,
        .is_equal = is_equal,

        .buckets = calloc(2, sizeof(struct bucket*)),
        .global_depth = 1,
        .dir_count = 2,
        .dirs = calloc(2, sizeof(struct bucket*)),
    };
    if (NULL == table->buckets || NULL == table->dirs) {
        goto free_buckets_dirs;
    }

    for (uint8_t i = 0; i < 1 + table->global_depth; ++i) {
        if (NULL == (table->buckets[i] = malloc(sizeof(struct bucket)))) {
            goto free_items_buckets;
        }
        *table->buckets[i] = (struct bucket){
            .size = 0,
            .local_depth = table->global_depth,
            .items = malloc(bucket_capacity * (key_size + value_size)),
        };
        if (NULL == table->buckets[i]->items) {
            goto free_items_buckets;
        }
        table->dirs[i] = table->buckets[i];
    }

    return table;

free_items_buckets:
    for (uint8_t i = 0; i < 1 + table->global_depth; ++i) {
        if (NULL != table->buckets[i]) {
            free(table->buckets[i]->items);
            free(table->buckets[i]);
        }
    }
free_buckets_dirs:
    free(table->buckets);
    free(table->dirs);
free_table:
    free(table);
    return NULL;
}

void
eh_destroy(eh_hashtable_t* table) {
    for (uint8_t i = 0; i < 1 + table->global_depth; ++i) {
        free(table->buckets[i]->items);
        free(table->buckets[i]);
    }
    free(table->buckets);
    free(table->dirs);
    free(table);
}
