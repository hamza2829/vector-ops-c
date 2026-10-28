#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Parallel arrays use 17 bytes per slot on an 8-bit-byte machine, before
 * allocator overhead. Occupancy is separate: every uint64_t key is valid.
 * Fields are visible for teaching/tests; callers must not mutate them. */
typedef struct {
    uint64_t *keys;
    uint64_t *values;
    unsigned char *occupied;
    size_t size;
    size_t capacity;
} HashTable;

uint64_t ht_hash(uint64_t key);
/* Rounds up to a power of two, with a minimum of eight. NULL on failure. */
HashTable *ht_create(size_t requested_capacity);
/* Inserts or updates. False on allocation failure or a NULL table.
 * A failed resize leaves the original table intact. */
bool ht_put(HashTable *table, uint64_t key, uint64_t value);
/* False for a missing key. out may be NULL for an existence check.
 * On a miss, an existing output value is left unchanged. */
bool ht_get(const HashTable *table, uint64_t key, uint64_t *out);
void ht_free(HashTable *table);

#endif
