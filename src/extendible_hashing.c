#include "extendible_hashing.h"

struct bucket {
        unsigned char depth_local;
        unsigned int item_count;
        char items[]; // [hash][key][val]
};

struct extendible_hashing_hashtable {
        struct bucket** buckets;
        size_t bucket_count;
        struct bucket** dirs;
        size_t dir_count;
        unsigned int bucket_capacity;
        unsigned char depth_global;

        size_t key_size;
        size_t val_size;
        size_t (*hash)(const void*);
        int (*cmp)(const void*, const void*);
};

eh_hashtable_t*
eh_create(size_t const key_size, size_t const val_size, unsigned int const bucket_capacity,
          size_t (*const hash)(const void*), int (*const cmp)(const void*, const void*)) {
        assert(key_size != 0 && val_size != 0 && bucket_capacity > 1 && hash != NULL
               && cmp != NULL);

        eh_hashtable_t* const table = malloc(sizeof(eh_hashtable_t));
        assert(table != NULL);
        *table = (eh_hashtable_t){
                .buckets = malloc(2 * sizeof(struct bucket*)),
                .bucket_count = 2,
                .dirs = malloc(2 * sizeof(struct bucket*)),
                .dir_count = 2,
                .bucket_capacity = bucket_capacity,
                .depth_global = 1,

                .key_size = key_size,
                .val_size = val_size,
                .hash = hash,
                .cmp = cmp,
        };
        assert(table->buckets != NULL && table->dirs != NULL);

        for (size_t i = 0; i < 2; ++i) {
                table->buckets[i] = malloc(sizeof(struct bucket)
                                           + bucket_capacity
                                                     * (sizeof(size_t) + key_size + val_size));
                assert(table->buckets[i] != NULL);
                *table->buckets[i] = (struct bucket){.depth_local = 1};

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

static inline size_t*
hash_ptr(const eh_hashtable_t* const table, const struct bucket* const bucket,
         unsigned int const i) {
        return (size_t*)(bucket->items + i * (sizeof(size_t) + table->key_size + table->val_size));
}

static inline void*
key_ptr(const eh_hashtable_t* const table, const struct bucket* const bucket,
        unsigned int const i) {
        return (char*)bucket->items + i * (sizeof(size_t) + table->key_size + table->val_size)
               + sizeof(size_t);
}

static inline void*
val_ptr(const eh_hashtable_t* const table, const struct bucket* const bucket,
        unsigned int const i) {
        return (char*)bucket->items + i * (sizeof(size_t) + table->key_size + table->val_size)
               + sizeof(size_t) + table->key_size;
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
split(eh_hashtable_t* const table, struct bucket* const bucket) {
        if (bucket->depth_local == table->depth_global) { // Expansion
                assert(table->depth_global <= 8 * sizeof(table->dir_count)
                       && "Increase bucket_capacity");
                table->dirs = realloc(table->dirs, 2 * table->dir_count * sizeof(struct bucket*));
                assert(table->dirs != NULL);

                for (size_t i = 0; i < table->dir_count; ++i) {
                        table->dirs[i + table->dir_count] = table->dirs[i];
                }
                table->dir_count *= 2;
                ++table->depth_global;
        }

        struct bucket* const new_bucket = malloc(
                sizeof(struct bucket)
                + table->bucket_capacity * (sizeof(size_t) + table->key_size + table->val_size));
        assert(new_bucket != NULL);
        *new_bucket = (struct bucket){.depth_local = bucket->depth_local + 1};

        const size_t high_bit = 1 << bucket->depth_local;
        const size_t hash = *hash_ptr(table, bucket, table->bucket_capacity - 1) & (high_bit - 1);
        ++bucket->depth_local;
        const unsigned int old_item_count = bucket->item_count;
        bucket->item_count = 0;

        for (unsigned int i = 0; i < old_item_count; ++i) { // Rehash
                const size_t* h = hash_ptr(table, bucket, i);
                struct bucket* const target = *h & high_bit ? new_bucket : bucket;

                memcpy(hash_ptr(table, target, target->item_count), h,
                       sizeof(size_t) + table->key_size + table->val_size);
                ++target->item_count;
        }

        for (size_t i = hash; i < table->dir_count; i += high_bit) {
                table->dirs[i] = i & high_bit ? new_bucket : bucket;
        }

        if ((table->bucket_count & (table->bucket_count - 1)) == 0) { // Is power of 2
                table->buckets = realloc(table->buckets,
                                         2 * table->bucket_count * sizeof(struct bucket*));
                assert(table->buckets != NULL);
        }
        table->buckets[table->bucket_count] = new_bucket;
        ++table->bucket_count;

        if (bucket->item_count == 0) {
                split(table, new_bucket);
        } else if (new_bucket->item_count == 0) {
                split(table, bucket);
        }
}

static inline struct bucket*
bucket_ptr(const eh_hashtable_t* const table, size_t const hash) {
        return table->dirs[hash & (((size_t)1 << table->depth_global) - 1)];
}

void
eh_insert(eh_hashtable_t* const table, const void* const key, const void* const val) {
        assert(table != NULL && key != NULL && val != NULL);

        const size_t hash = table->hash(key);
        struct bucket* const bucket = bucket_ptr(table, hash);
        for (unsigned int i = 0; i < bucket->item_count; ++i) {
                if (*hash_ptr(table, bucket, i) == hash
                    && table->cmp(key_ptr(table, bucket, i), key) == 0) {
                        memcpy(val_ptr(table, bucket, i), val, table->val_size);
                        return; // Update
                }
        }

        *hash_ptr(table, bucket, bucket->item_count) = hash;
        memcpy(key_ptr(table, bucket, bucket->item_count), key, table->key_size);
        memcpy(val_ptr(table, bucket, bucket->item_count), val, table->val_size);
        ++bucket->item_count;
        if (bucket->item_count < table->bucket_capacity) {
                return; // Insert
        }

        split(table, bucket);
}

void*
eh_lookup(const eh_hashtable_t* const table, const void* const key) {
        assert(table != NULL && key != NULL);

        const size_t hash = table->hash(key);
        const struct bucket* const bucket = bucket_ptr(table, hash);
        for (unsigned int i = 0; i < bucket->item_count; ++i) {
                if (*hash_ptr(table, bucket, i) == hash
                    && table->cmp(key_ptr(table, bucket, i), key) == 0) {
                        return val_ptr(table, bucket, i);
                }
        }
        return NULL;
}

void
eh_erase(eh_hashtable_t* const table, const void* const key) {
        assert(table != NULL && key != NULL);

        const size_t hash = table->hash(key);
        struct bucket* const bucket = bucket_ptr(table, hash);
        for (unsigned int i = 0; i < bucket->item_count; ++i) {
                if (*hash_ptr(table, bucket, i) == hash
                    && table->cmp(key_ptr(table, bucket, i), key) == 0) { // Shift
                        memmove(hash_ptr(table, bucket, i),
                                hash_ptr(table, bucket, bucket->item_count - 1),
                                sizeof(size_t) + table->key_size + table->val_size);
                        --bucket->item_count;
                        return;
                }
        }
}
