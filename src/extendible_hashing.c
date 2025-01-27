#include <assert.h>
#include <limits.h>
#include <string.h>
#include "extendible_hashing.h"

typedef unsigned char byte_t;

struct bucket {
        byte_t depth_local;
        byte_t mark;
        unsigned item_count;
        byte_t items[]; // [hash][key][val]
};

#define ITEM_SIZE(table) (sizeof(EH_HASH_T) + (table)->key_size + (table)->val_size)

struct extendible_hashing_hashtable {
        struct bucket** dirs;
        size_t dir_count;
        byte_t depth_global;

        unsigned bucket_capacity;
        size_t key_size;
        size_t val_size;
        EH_HASH_T (*hash)(const void*);
        int (*cmp)(const void*, const void*);
};

static inline void*
xmalloc(size_t const size) {
        void* const ptr = malloc(size);
        assert(ptr != NULL);
        return ptr;
}

static inline void*
xrealloc(void* const ptr, size_t const size) {
        void* const tmp = realloc(ptr, size);
        assert(tmp != NULL);
        return tmp;
}

static inline struct bucket*
create_bucket(const eh_hashtable_t* const table, byte_t const depth) {
        struct bucket* const bucket = xmalloc(sizeof(*table->dirs)
                                              + table->bucket_capacity * ITEM_SIZE(table));
        *bucket = (struct bucket){.depth_local = depth};
        return bucket;
}

eh_hashtable_t*
eh_create(size_t const key_size, size_t const val_size, unsigned const bucket_capacity,
          EH_HASH_T (*const hash)(const void*), int (*const cmp)(const void*, const void*)) {
        assert(key_size != 0 && val_size != 0 && bucket_capacity > 1 && hash != NULL);

        eh_hashtable_t* const table = xmalloc(sizeof(*table));
        const byte_t init_depth = 1;
        const size_t init_count = 1 << init_depth;
        *table = (eh_hashtable_t){
                .dirs = xmalloc(init_count * sizeof(*table->dirs)),
                .dir_count = init_count,
                .depth_global = init_depth,

                .bucket_capacity = bucket_capacity,
                .key_size = key_size,
                .val_size = val_size,
                .hash = hash,
                .cmp = cmp,
        };

        for (size_t i = 0; i < init_count; ++i) {
                table->dirs[i] = create_bucket(table, init_depth);
        }
        return table;
}

static inline void
reset_buckets(eh_hashtable_t* const table) {
        for (size_t i = 0; i < table->dir_count; ++i) {
                table->dirs[i]->mark = UCHAR_MAX;
        }
}

void
eh_destroy(eh_hashtable_t* const table) {
        assert(table != NULL);

        reset_buckets(table);
        struct bucket** const buckets = xmalloc(table->dir_count * sizeof(*table->dirs));
        size_t bucket_count = 0;

        for (size_t i = 0; i < table->dir_count; ++i) {
                if (table->dirs[i]->mark) {
                        buckets[bucket_count++] = table->dirs[i];
                        table->dirs[i]->mark = 0;
                }
        }
        for (size_t i = 0; i < bucket_count; ++i) {
                free(buckets[i]);
        }
        free(buckets);

        free(table->dirs);
        free(table);
}

eh_iterator_t
eh_iter(eh_hashtable_t* const table) {
        assert(table != NULL);

        reset_buckets(table);
        return (eh_iterator_t){.table = table};
}

static inline EH_HASH_T*
hash_ptr(const eh_hashtable_t table[static 1], const struct bucket* const bucket,
         unsigned const i) {
        return (EH_HASH_T*)(bucket->items + i * ITEM_SIZE(table));
}

static inline void*
key_ptr(const eh_hashtable_t table[static 1], const struct bucket* const bucket, unsigned const i) {
        return (byte_t*)bucket->items + i * ITEM_SIZE(table) + sizeof(EH_HASH_T);
}

static inline void*
val_ptr(const eh_hashtable_t table[static 1], const struct bucket* const bucket, unsigned const i) {
        return (byte_t*)bucket->items + i * ITEM_SIZE(table) + sizeof(EH_HASH_T) + table->key_size;
}

int
eh_next(eh_iterator_t* const it, const void** const key, const void** const val) {
        assert(it != NULL && key != NULL && val != NULL);

        while (it->dir_index < it->table->dir_count) {
                struct bucket* const bucket = it->table->dirs[it->dir_index];
                if (bucket->mark && it->item_index < bucket->item_count) {
                        *key = key_ptr(it->table, bucket, it->item_index);
                        *val = val_ptr(it->table, bucket, it->item_index);

                        if (++it->item_index == bucket->item_count) {
                                it->item_index = 0;
                                ++it->dir_index;
                                bucket->mark = 0;
                        }
                        return 1;
                }
                ++it->dir_index;
        }
        return 0;
}

static inline void
expansion(eh_hashtable_t table[static 1], byte_t const depth) {
        if (depth == table->depth_global) {
                table->dirs = xrealloc(table->dirs, 2 * table->dir_count * sizeof(table->dirs));
                for (size_t i = 0; i < table->dir_count; ++i) {
                        table->dirs[i + table->dir_count] = table->dirs[i];
                }
                table->dir_count *= 2;

                ++table->depth_global;
                assert(table->depth_global < CHAR_BIT * sizeof(table->dir_count)
                       && "Increase bucket_capacity");
                assert(table->depth_global < CHAR_BIT * sizeof(EH_HASH_T)
                       && "Increase EH_HASH_T byte size");
        }
}

static inline void
rehash(eh_hashtable_t table[static 1], struct bucket* const bucket,
       struct bucket* const new_bucket) {
        const EH_HASH_T high_bit = 1 << bucket->depth_local++;
        const EH_HASH_T hash = *hash_ptr(table, bucket, table->bucket_capacity - 1)
                               & (high_bit - 1);

        const unsigned old_item_count = bucket->item_count;
        bucket->item_count = 0;
        for (unsigned i = 0; i < old_item_count; ++i) {
                const EH_HASH_T* h = hash_ptr(table, bucket, i);
                struct bucket* const target = (*h & high_bit) ? new_bucket : bucket;
                memcpy(hash_ptr(table, target, target->item_count++), h, ITEM_SIZE(table));
        }

        for (EH_HASH_T i = hash; i < table->dir_count; i += high_bit) {
                table->dirs[i] = (i & high_bit) ? new_bucket : bucket;
        }
}

static void
split(eh_hashtable_t table[static 1], struct bucket* const bucket) {
        expansion(table, bucket->depth_local);

        struct bucket* const new_bucket = create_bucket(table, bucket->depth_local + 1);

        rehash(table, bucket, new_bucket);

        if (bucket->item_count == 0) {
                split(table, new_bucket);
        } else if (new_bucket->item_count == 0) {
                split(table, bucket);
        }
}

static inline struct bucket*
bucket_ptr(const eh_hashtable_t table[static 1], EH_HASH_T const hash) {
        return table->dirs[hash & (((EH_HASH_T)1 << table->depth_global) - 1)];
}

static inline int
key_cmp(const eh_hashtable_t table[static 1], const struct bucket* const bucket, unsigned const i,
        EH_HASH_T const hash, const void* const key) {
        return *hash_ptr(table, bucket, i) == hash
               && (table->cmp == NULL ? memcmp(key_ptr(table, bucket, i), key, table->key_size)
                                      : table->cmp(key_ptr(table, bucket, i), key))
                          == 0;
}

void
eh_insert(eh_hashtable_t* const table, const void* const key, const void* const val) {
        assert(table != NULL && key != NULL && val != NULL);

        const EH_HASH_T hash = table->hash(key);
        struct bucket* const bucket = bucket_ptr(table, hash);
        for (unsigned i = 0; i < bucket->item_count; ++i) {
                if (key_cmp(table, bucket, i, hash, key)) {
                        memcpy(val_ptr(table, bucket, i), val, table->val_size);
                        return; // Update
                }
        }

        *hash_ptr(table, bucket, bucket->item_count) = hash;
        memcpy(key_ptr(table, bucket, bucket->item_count), key, table->key_size);
        memcpy(val_ptr(table, bucket, bucket->item_count), val, table->val_size);
        if (++bucket->item_count < table->bucket_capacity) {
                return; // Insert
        }

        split(table, bucket);
}

void*
eh_lookup(const eh_hashtable_t* const table, const void* const key) {
        assert(table != NULL && key != NULL);

        const EH_HASH_T hash = table->hash(key);
        const struct bucket* const bucket = bucket_ptr(table, hash);
        for (unsigned i = 0; i < bucket->item_count; ++i) {
                if (key_cmp(table, bucket, i, hash, key)) {
                        return val_ptr(table, bucket, i);
                }
        }
        return NULL;
}

void
eh_erase(eh_hashtable_t* const table, const void* const key) {
        assert(table != NULL && key != NULL);

        const EH_HASH_T hash = table->hash(key);
        struct bucket* const bucket = bucket_ptr(table, hash);
        for (unsigned i = 0; i < bucket->item_count; ++i) {
                if (key_cmp(table, bucket, i, hash, key)) { // Shift
                        memmove(hash_ptr(table, bucket, i),
                                hash_ptr(table, bucket, bucket->item_count-- - 1),
                                ITEM_SIZE(table));
                        return;
                }
        }
}
