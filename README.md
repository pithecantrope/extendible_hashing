# 258 LoC

## API

`eh_hashtable_t* eh_create(size_t key_size, size_t value_size, uint16_t bucket_capacity, uint64_t (*const hash)(const void*, size_t), bool (*const is_equal)(const void*, const void*, size_t))`

`void eh_destroy(eh_hashtable_t* table)`

`void eh_insert(eh_hashtable_t* table, const void* key, const void* value)`

`void eh_erase(eh_hashtable_t* table, const void* key)`

`void* eh_lookup(const eh_hashtable_t* table, const void* key)`

`eh_iterator_t eh_iter(const eh_hashtable_t* table)`

`bool eh_next(eh_iterator_t* iterator, const void** key, const void** value)`

## Usage
- Add `extendible_hashing.c` and `extendible_hashing.h` to your project's `src/` directory
- Include the header file
```c
#include <extendible_hashing.h>
```
