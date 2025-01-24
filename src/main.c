#include <stdio.h>
#include <string.h>
#include "extendible_hashing.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

EH_HASH_T
hash_fnv1a(const void* key) {
        EH_HASH_T hash = FNV_OFFSET;
        for (const char* p = key; *p; p++) {
                hash ^= (EH_HASH_T)*p;
                hash *= FNV_PRIME;
        }
        return hash;
}

int
cmp(const void* key1, const void* key2) {
        return strcmp(key1, key2);
}

#define BUFFER_SIZE 4096
#define WORD_SIZE   32

int
main(int argc, char* argv[]) {
        unsigned bucket_capacity;
        if (argc != 2 || sscanf(argv[1], "%u", &bucket_capacity) != 1) {
                fprintf(stderr, "Usage: %s <bucket_capacity>\n", *argv);
                return EXIT_FAILURE;
        }

        const char path[] = "res/data.txt";
        FILE* const file = fopen(path, "rt");
        if (file == NULL) {
                fprintf(stderr, "Could not open %s for reading\n", path);
                return EXIT_FAILURE;
        }

        eh_hashtable_t* const table = eh_create(WORD_SIZE, sizeof(EH_HASH_T), bucket_capacity,
                                                hash_fnv1a, cmp);
        for (char buffer[BUFFER_SIZE]; fgets(buffer, BUFFER_SIZE, file) != NULL;) {
                for (const char* word = strtok(buffer, "\n"); word != NULL;
                     word = strtok(NULL, "\n")) {
                        const void* const value = eh_lookup(table, word);
                        if (value == NULL) {
                                eh_insert(table, word, &(EH_HASH_T){1});
                        } else {
                                ++*(EH_HASH_T*)value;
                        }
                }
        }

        size_t count = 0;
        const void *key, *val;
        EH_FOREACH_ITEM(table, key, val) {
                if (*(EH_HASH_T*)val >= 1024) {
                        ++count;
                }
        }
        printf("%zu words with >= 1024 repetitions\n", count);

        eh_destroy(table);
        fclose(file);
        return EXIT_SUCCESS;
}
