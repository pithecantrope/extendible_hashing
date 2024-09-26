#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extendible_hashing.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

uint64_t
hash_fnv1a(const void* key, size_t key_size) {
    (void)key_size;
    uint64_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (uint64_t)*p;
        hash *= FNV_PRIME;
    }
    return hash;
}

bool
is_equal_str(const void* key1, const void* key2, size_t key_size) {
    (void)key_size;
    return 0 == strcmp(key1, key2);
}

int
main(int argc, char* argv[]) {
    uint16_t bucket_capacity;
    if (2 != argc || 1 != sscanf(argv[1], "%" SCNu16, &bucket_capacity)) {
        fprintf(stderr, "Usage: %s <bucket_capacity>\n", *argv);
        return EXIT_FAILURE;
    }

    const char path[] = "res/data.txt";
    FILE* file = fopen(path, "rt");
    if (NULL == file) {
        fprintf(stderr, "Could not open %s for reading\n", path);
        return EXIT_FAILURE;
    }

    char word[64];
    eh_hashtable_t* table = eh_create(sizeof(word), sizeof(size_t), bucket_capacity, hash_fnv1a, is_equal_str);
    while (1 == fscanf(file, "%s\n", word)) {
        void* value = eh_lookup(table, word);
        if (NULL == value) {
            size_t init_value = 1;
            eh_insert(table, word, &init_value);
        } else {
            ++*(size_t*)value;
        }
    }
    eh_stat(table);

    size_t count = 0;
    void *key, *val;
    for (eh_iterator_t iterator = eh_iter(table); eh_next(&iterator, &key, &val);) {
            // printf("%s\n", (char*)key);
        if (*(size_t*)val >= 1024) {
            ++count;
        }
    }
    printf("%zu words with >= 1024 repetitions\n", count);

    eh_hashtable_t* new_table = eh_create(sizeof(word), sizeof(size_t), bucket_capacity, hash_fnv1a, is_equal_str);
    eh_insert(new_table, word, &(size_t){1});

    eh_erase(new_table, word);
    printf("%s\n", word);
    strcpy(word, "Hello");
    eh_insert(new_table, word, &(size_t){1});
    eh_erase(new_table, word);
    printf("%s\n", word);

    eh_destroy(new_table);
    eh_destroy(table);
    fclose(file);
    return EXIT_SUCCESS;
}
