#include "extendible_hashing.h"
#include <stdio.h>

#define FIELD_SIZEOF(type, field) (sizeof(((type*)0)->field))

struct bucket {
    uint16_t item_count;
    uint8_t depth_local;
    void* items;
};

struct extendible_hashing_hashtable {
    size_t key_size;
    size_t val_size;
    uint64_t (*hash)(const void*, size_t);
    bool (*is_equal)(const void*, const void*, size_t);

    uint32_t bucket_count;
    uint16_t bucket_capacity;
    uint8_t depth_global;
    struct bucket** buckets;
    size_t dir_count;
    struct bucket** dirs;
};

static_assert(FIELD_SIZEOF(struct bucket, item_count)
                  == FIELD_SIZEOF(struct extendible_hashing_hashtable, bucket_capacity),
              "A bucket is a container of items");
static_assert(FIELD_SIZEOF(struct bucket, depth_local)
                  == FIELD_SIZEOF(struct extendible_hashing_hashtable, depth_global),
              "local depth <= global depth");
static_assert(FIELD_SIZEOF(struct extendible_hashing_hashtable, bucket_count)
                  < FIELD_SIZEOF(struct extendible_hashing_hashtable, dir_count),
              "Multiple directories can point to the same bucket");

eh_hashtable_t*
eh_create(size_t key_size, size_t val_size, uint16_t bucket_capacity, uint64_t (*const hash)(const void*, size_t),
          bool (*const is_equal)(const void*, const void*, size_t)) {
    assert(0 != key_size && 0 != val_size && 1 < bucket_capacity && NULL != hash && NULL != is_equal);

    eh_hashtable_t* table = malloc(sizeof(eh_hashtable_t));
    assert(NULL != table);
    const uint8_t init_depth_global = 1;
    const uint32_t init_bucket_count = 1 << init_depth_global;

    *table = (eh_hashtable_t){
        .key_size = key_size,
        .val_size = val_size,
        .hash = hash,
        .is_equal = is_equal,

        .bucket_count = init_bucket_count,
        .bucket_capacity = bucket_capacity,
        .depth_global = init_depth_global,
        .buckets = malloc(init_bucket_count * sizeof(struct bucket*)),
        .dir_count = init_bucket_count,
        .dirs = malloc(init_bucket_count * sizeof(struct bucket*)),
    };
    assert(NULL != table->buckets && NULL != table->dirs);

    for (uint32_t i = 0; i < init_bucket_count; ++i) {
        table->buckets[i] = malloc(sizeof(struct bucket));
        assert(NULL != table->buckets[i]);

        *table->buckets[i] = (struct bucket){
            .item_count = 0,
            .depth_local = init_depth_global,
            .items = malloc(bucket_capacity * (key_size + val_size)),
        };
        assert(NULL != table->buckets[i]->items);

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
key_ptr(const eh_hashtable_t* table, const struct bucket* bucket, uint16_t i) {
    return (char*)bucket->items + (i * table->key_size);
}

[[nodiscard]] static inline void*
val_ptr(const eh_hashtable_t* table, const struct bucket* bucket, uint16_t i) {
    return (char*)bucket->items + (table->bucket_capacity * table->key_size) + (i * table->val_size);
}

static_assert(FIELD_SIZEOF(eh_iterator_t, item_index) == FIELD_SIZEOF(struct bucket, item_count),
              "item_index <= item_count");
static_assert(FIELD_SIZEOF(eh_iterator_t, bucket_index) == FIELD_SIZEOF(eh_hashtable_t, bucket_count),
              "bucket_index <= bucket_count");

eh_iterator_t
eh_iter(const eh_hashtable_t* table) {
    assert(NULL != table);

    return (eh_iterator_t){
        .table = table,
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
    *key = key_ptr(iterator->table, bucket, iterator->item_index);
    *val = val_ptr(iterator->table, bucket, iterator->item_index);

    ++iterator->item_index;

    if (iterator->item_index == bucket->item_count) {
        iterator->item_index = 0;
        ++iterator->bucket_index;
    }
    return true;
}

static void
split(eh_hashtable_t* table, const void* key, struct bucket* bucket) {
    if (bucket->depth_local == table->depth_global) { // Expansion
        table->dirs = realloc(table->dirs, 2 * table->dir_count * sizeof(struct bucket*));
        assert(NULL != table->dirs);

        for (size_t i = 0; i < table->dir_count; ++i) {
            table->dirs[i + table->dir_count] = table->dirs[i];
        }
        table->dir_count *= 2;
        ++table->depth_global;
    }

    struct bucket* new_bucket = malloc(sizeof(struct bucket));
    assert(NULL != new_bucket);
    *new_bucket = (struct bucket){
        .item_count = 0,
        .depth_local = 1 + bucket->depth_local,
        .items = malloc(table->bucket_capacity * (table->key_size + table->val_size)),
    };
    assert(NULL != new_bucket->items);

    uint64_t high_bit = 1 << bucket->depth_local;
    ++bucket->depth_local;
    uint16_t old_item_count = bucket->item_count;
    bucket->item_count = 0;

    for (uint16_t i = 0; i < old_item_count; ++i) { // Rehash:
        void* k = key_ptr(table, bucket, i);
        void* v = val_ptr(table, bucket, i);
        struct bucket* target = table->hash(k, table->key_size) & high_bit ? new_bucket : bucket;
        memcpy(key_ptr(table, target, target->item_count), k, table->key_size);
        memcpy(val_ptr(table, target, target->item_count), v, table->val_size);
        ++target->item_count;
    }

    for (size_t i = table->hash(key, table->key_size) & (high_bit - 1); i < table->dir_count; i += high_bit) {
        table->dirs[i] = i & high_bit ? new_bucket : bucket;
    }

    if (0 == (table->bucket_count & (table->bucket_count - 1))) { // Is power of 2
        table->buckets = realloc(table->buckets, 2 * table->bucket_count * sizeof(struct bucket*));
        assert(NULL != table->buckets);
    }
    table->buckets[table->bucket_count] = new_bucket;
    ++table->bucket_count;

    if (bucket->item_count == 0) {
        split(table, key, new_bucket);
    }
    if (new_bucket->item_count == 0) {
        split(table, key, bucket);
    }
}

[[nodiscard]] static struct bucket*
bucket_ptr(const eh_hashtable_t* table, const void* key) {
    return table->dirs[table->hash(key, table->key_size) & ((1 << table->depth_global) - 1)];
}

void
eh_insert(eh_hashtable_t* table, const void* key, const void* val) {
    assert(NULL != table && NULL != key && NULL != val);

    struct bucket* bucket = bucket_ptr(table, key);
    for (uint16_t i = 0; i < bucket->item_count; ++i) {
        if (table->is_equal(key_ptr(table, bucket, i), key, table->key_size)) {
            memcpy(val_ptr(table, bucket, i), val, table->val_size);
            return; // Update
        }
    }

    memcpy(key_ptr(table, bucket, bucket->item_count), key, table->key_size);
    memcpy(val_ptr(table, bucket, bucket->item_count), val, table->val_size);
    ++bucket->item_count;
    if (bucket->item_count < table->bucket_capacity) {
        return; // Insert
    }

    split(table, key, bucket);
}

void*
eh_lookup(const eh_hashtable_t* table, const void* key) {
    assert(NULL != table && NULL != key);

    struct bucket* bucket = bucket_ptr(table, key);
    for (uint16_t i = 0; i < bucket->item_count; ++i) {
        if (table->is_equal(key_ptr(table, bucket, i), key, table->key_size)) {
            return val_ptr(table, bucket, i);
        }
    }
    return NULL;
}

void
eh_erase(eh_hashtable_t* table, const void* key) {
    assert(NULL != table && NULL != key);

    struct bucket* bucket = bucket_ptr(table, key);
    for (uint16_t i = 0; i < bucket->item_count; ++i) {
        if (table->is_equal(key_ptr(table, bucket, i), key, table->key_size)) {
            for (uint16_t j = i; j < bucket->item_count - 1; ++j) { // Shift
                memcpy(key_ptr(table, bucket, j), key_ptr(table, bucket, j + 1), table->key_size);
                memcpy(val_ptr(table, bucket, j), val_ptr(table, bucket, j + 1), table->val_size);
            }
            --bucket->item_count;
            return;
        }
    }
}

void
eh_stat(const eh_hashtable_t* table) {
    printf("bucket_capacity: %hu bucket_count: %u dir_count: %zu depth_global: %hhu key_size:  %zu value_size: %zu\n",
           table->bucket_capacity, table->bucket_count, table->dir_count, table->depth_global, table->key_size,
           table->val_size);
}
