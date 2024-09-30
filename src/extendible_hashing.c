#include "extendible_hashing.h"

struct bucket {
    unsigned char depth_local;
    unsigned int item_count;
    void* items[];
};

struct extendible_hashing_hashtable {
    struct bucket** buckets;
    size_t bucket_count;
    struct bucket** dirs;
    size_t dir_count;

    size_t key_size;
    size_t val_size;
    size_t (*hash)(const void*);
    int (*cmp)(const void*, const void*);
    unsigned int bucket_capacity;
    unsigned char depth_global;
};

eh_hashtable_t*
eh_create(size_t const key_size, size_t const val_size, unsigned int const bucket_capacity,
          size_t (*const hash)(const void*), int (*const cmp)(const void*, const void*)) {
    assert(key_size != 0 && val_size != 0 && bucket_capacity > 1 && hash != NULL && cmp != NULL);

    eh_hashtable_t* const table = malloc(sizeof(eh_hashtable_t));
    assert(table != NULL);
    const unsigned char init_depth_global = 1;
    const size_t init_bucket_count = 1 << init_depth_global;

    *table = (eh_hashtable_t){
        .buckets = malloc(init_bucket_count * sizeof(struct bucket*)),
        .bucket_count = init_bucket_count,
        .dirs = malloc(init_bucket_count * sizeof(struct bucket*)),
        .dir_count = init_bucket_count,

        .key_size = key_size,
        .val_size = val_size,
        .hash = hash,
        .cmp = cmp,
        .bucket_capacity = bucket_capacity,
        .depth_global = init_depth_global,
    };
    assert(table->buckets != NULL && table->dirs != NULL);

    for (size_t i = 0; i < init_bucket_count; ++i) {
        table->buckets[i] = malloc(sizeof(struct bucket) + bucket_capacity * (key_size + val_size));
        assert(table->buckets[i] != NULL);
        *table->buckets[i] = (struct bucket){.depth_local = init_depth_global};

        table->dirs[i] = table->buckets[i];
    }

    return table;
}

void
eh_destroy(eh_hashtable_t* const table) {
    assert(table != NULL);

    for (size_t i = 0; i < table->bucket_count; ++i) {
        free(table->buckets[i]);
    }
    free(table->buckets);
    free(table->dirs);
    free(table);
}

static inline void*
key_ptr(const eh_hashtable_t* const table, const struct bucket* const bucket, unsigned int const i) {
    return (char*)bucket->items + (i * table->key_size);
}

static inline void*
val_ptr(const eh_hashtable_t* const table, const struct bucket* const bucket, unsigned int const i) {
    return (char*)bucket->items + (table->bucket_capacity * table->key_size) + (i * table->val_size);
}

int
eh_next(eh_iterator_t* const iterator, const void** const key, const void** const val) {
    assert(iterator != NULL && key != NULL && val != NULL);

    const struct bucket* bucket;
    do { // Skip empty buckets
        if (iterator->bucket_index == iterator->table->bucket_count) {
            return 0;
        }

        bucket = iterator->table->buckets[iterator->bucket_index];
    } while (0 == bucket->item_count && ++iterator->bucket_index);

    *key = key_ptr(iterator->table, bucket, iterator->item_index);
    *val = val_ptr(iterator->table, bucket, iterator->item_index);
    ++iterator->item_index;

    if (iterator->item_index == bucket->item_count) {
        iterator->item_index = 0;
        ++iterator->bucket_index;
    }
    return 1;
}

static void
split(eh_hashtable_t* const table, const void* const key, struct bucket* const bucket) {
    if (bucket->depth_local == table->depth_global) { // Expansion
        assert(table->depth_global != 8 * sizeof(table->dir_count) && "Increase bucket_capacity");
        table->dirs = realloc(table->dirs, 2 * table->dir_count * sizeof(struct bucket*));
        assert(table->dirs != NULL);

        for (size_t i = 0; i < table->dir_count; ++i) {
            table->dirs[i + table->dir_count] = table->dirs[i];
        }
        table->dir_count *= 2;
        ++table->depth_global;
    }

    struct bucket* const new_bucket = malloc(sizeof(struct bucket)
                                             + table->bucket_capacity * (table->key_size + table->val_size));
    assert(new_bucket != NULL);
    *new_bucket = (struct bucket){.depth_local = bucket->depth_local + 1};

    const size_t high_bit = 1 << bucket->depth_local;
    ++bucket->depth_local;
    const unsigned int old_item_count = bucket->item_count;
    bucket->item_count = 0;

    for (unsigned int i = 0; i < old_item_count; ++i) { // Rehash
        const void* k = key_ptr(table, bucket, i);
        const void* v = val_ptr(table, bucket, i);
        struct bucket* const target = table->hash(k) & high_bit ? new_bucket : bucket;

        memcpy(key_ptr(table, target, target->item_count), k, table->key_size);
        memcpy(val_ptr(table, target, target->item_count), v, table->val_size);
        ++target->item_count;
    }

    for (size_t i = table->hash(key) & (high_bit - 1); i < table->dir_count; i += high_bit) {
        table->dirs[i] = i & high_bit ? new_bucket : bucket;
    }

    if (0 == (table->bucket_count & (table->bucket_count - 1))) { // Is power of 2
        table->buckets = realloc(table->buckets, 2 * table->bucket_count * sizeof(struct bucket*));
        assert(table->buckets != NULL);
    }
    table->buckets[table->bucket_count] = new_bucket;
    ++table->bucket_count;

    if (0 == bucket->item_count) {
        split(table, key, new_bucket);
    } else if (0 == new_bucket->item_count) {
        split(table, key, bucket);
    }
}

static struct bucket*
bucket_ptr(const eh_hashtable_t* const table, const void* const key) {
    return table->dirs[table->hash(key) & (((size_t)1 << table->depth_global) - 1)];
}

void
eh_insert(eh_hashtable_t* const table, const void* const key, const void* const val) {
    assert(table != NULL && key != NULL && val != NULL);

    struct bucket* const bucket = bucket_ptr(table, key);
    for (unsigned int i = 0; i < bucket->item_count; ++i) {
        if (table->cmp(key_ptr(table, bucket, i), key) == 0) {
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
eh_lookup(const eh_hashtable_t* const table, const void* const key) {
    assert(table != NULL && key != NULL);

    const struct bucket* const bucket = bucket_ptr(table, key);
    for (unsigned int i = 0; i < bucket->item_count; ++i) {
        if (table->cmp(key_ptr(table, bucket, i), key) == 0) {
            return val_ptr(table, bucket, i);
        }
    }
    return NULL;
}

void
eh_erase(eh_hashtable_t* const table, const void* const key) {
    assert(table != NULL && key != NULL);

    struct bucket* const bucket = bucket_ptr(table, key);
    for (unsigned int i = 0; i < bucket->item_count; ++i) {
        if (table->cmp(key_ptr(table, bucket, i), key) == 0) { // Shift
            memcpy(key_ptr(table, bucket, i), key_ptr(table, bucket, i + 1),
                   (bucket->item_count - i - 1) * table->key_size);
            memcpy(val_ptr(table, bucket, i), val_ptr(table, bucket, i + 1),
                   (bucket->item_count - i - 1) * table->val_size);
            --bucket->item_count;
            return;
        }
    }
}
