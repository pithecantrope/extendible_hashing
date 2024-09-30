#include <stdio.h>
#include "extendible_hashing.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

size_t
hash_fnv1a(const void* key) {
    size_t hash = FNV_OFFSET;
    for (const char* p = key; *p; p++) {
        hash ^= (size_t)*p;
        hash *= FNV_PRIME;
    }
    return hash;
}

int
cmp(const void* key1, const void* key2) {
    return strcmp(key1, key2);
}

int
main(int argc, char* argv[]) {
    unsigned int bucket_capacity;
    if (2 != argc || 1 != sscanf(argv[1], "%u", &bucket_capacity)) {
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
    eh_hashtable_t* table = eh_create(sizeof(word), sizeof(size_t), bucket_capacity, hash_fnv1a, cmp);
    const size_t init_value = 1;
    while (1 == fscanf(file, "%s\n", word)) {
        void* value = eh_lookup(table, word);
        if (NULL == value) {
            eh_insert(table, word, &init_value);
        } else {
            ++*(size_t*)value;
        }
    }

    size_t count = 0;
    const void *key, *val;
    for (eh_iterator_t iterator = eh_iter(table); eh_next(&iterator, &key, &val);) {
        if (*(size_t*)val >= 1024) {
            ++count;
        }
    }
    printf("%zu words with >= 1024 repetitions\n", count);

    eh_destroy(table);
    fclose(file);
    return EXIT_SUCCESS;
}
