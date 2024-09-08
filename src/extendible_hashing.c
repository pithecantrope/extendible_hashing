#include "extendible_hashing.h"

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
    uint16_t bucket_capacity;
    uint8_t global_depth;
    struct bucket** buckets;
    size_t dir_count;
    struct bucket** dirs;
};

eh_hashtable_t*
eh_create(uint16_t bucket_capacity, size_t key_size, size_t val_size, uint64_t (*const hash)(const void*, size_t),
          bool (*const is_equal)(const void*, const void*, size_t)) {
    if (0 == bucket_capacity || 0 == key_size || 0 == val_size || NULL == hash || NULL == is_equal) {
        return NULL;
    }

    eh_hashtable_t* table = malloc(sizeof(eh_hashtable_t));
    if (NULL == table) {
        goto free_table;
    }
    *table = (eh_hashtable_t){
        .key_size = key_size,
        .val_size = val_size,
        .hash = hash,
        .is_equal = is_equal,

        .bucket_count = 2,
        .bucket_capacity = bucket_capacity,
        .global_depth = 1,
        .buckets = calloc(2, sizeof(struct bucket*)),
        .dir_count = 2,
        .dirs = calloc(2, sizeof(struct bucket*)),
    };
    if (NULL == table->buckets || NULL == table->dirs) {
        goto free_buckets_dirs;
    }

    for (uint32_t i = 0; i < table->bucket_count; ++i) {
        if (NULL == (table->buckets[i] = malloc(sizeof(struct bucket)))) {
            goto free_items_buckets;
        }
        *table->buckets[i] = (struct bucket){
            .size = 0,
            .local_depth = 1,
            .items = calloc(bucket_capacity, key_size + val_size),
        };
        if (NULL == table->buckets[i]->items) {
            goto free_items_buckets;
        }
        table->dirs[i] = table->buckets[i];
    }

    return table;

free_items_buckets:
    for (uint32_t i = 0; i < table->bucket_count; ++i) {
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

struct extendible_hashing_iterator {
    eh_hashtable_t* table;
    uint32_t bucket_index;
    uint16_t item_index;
};

eh_iterator_t
eh_iter(eh_hashtable_t* table) {
    return (eh_iterator_t){
        .table = table,
        .bucket_index = 0,
        .item_index = 0,
    };
}

bool
eh_next(eh_iterator_t* iterator, void** key, void** val) {
    if (NULL == iterator || NULL == key || NULL == val) {
        return false;
    }

    struct bucket* bucket = iterator->table->buckets[iterator->bucket_index];
    *key = get_key_ptr(iterator->table, bucket, iterator->item_index);
    *val = get_val_ptr(iterator->table, bucket, iterator->item_index);

    if (++iterator->item_index < iterator->table->bucket_capacity) {
        return true;
    }
    iterator->item_index = 0;
    if (++iterator->bucket_index < iterator->table->bucket_count) {
        return true;
    }

    return false;
}
