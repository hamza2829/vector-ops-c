#include "hash_table.h"

#include <stdlib.h>

uint64_t ht_hash(uint64_t key)
{
    /* SplitMix64-style mixing. Unsigned overflow is intentional and defined.
     * This is a deterministic hash, not a cryptographic one. */
    key += UINT64_C(0x9e3779b97f4a7c15);
    key = (key ^ (key >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    key = (key ^ (key >> 27)) * UINT64_C(0x94d049bb133111eb);
    return key ^ (key >> 31);
}

HashTable *ht_create(size_t requested_capacity)
{
    size_t capacity = 8;
    HashTable *table;

    while (capacity < requested_capacity) {
        if (capacity > SIZE_MAX / 2) {
            return NULL;
        }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(uint64_t)) {
        return NULL;
    }
    table = calloc(1, sizeof(*table));
    if (table == NULL) {
        return NULL;
    }
    table->keys = malloc(capacity * sizeof(*table->keys));
    table->values = malloc(capacity * sizeof(*table->values));
    table->occupied = calloc(capacity, sizeof(*table->occupied));
    if (table->keys == NULL || table->values == NULL || table->occupied == NULL) {
        ht_free(table);
        return NULL;
    }
    table->capacity = capacity;
    return table;
}

static size_t find_slot(const HashTable *table, uint64_t key)
{
    size_t mask = table->capacity - 1;
    size_t slot = (size_t)ht_hash(key) & mask;

    /* Below 70% load there is always an empty slot. Linear probing wraps. */
    while (table->occupied[slot] && table->keys[slot] != key) {
        slot = (slot + 1) & mask;
    }
    return slot;
}

static void insert_without_resize(HashTable *table, uint64_t key, uint64_t value)
{
    size_t slot = find_slot(table, key);
    table->keys[slot] = key;
    table->values[slot] = value;
    table->occupied[slot] = 1;
    table->size++;
}

static bool grow(HashTable *table)
{
    HashTable *larger;
    size_t i;

    if (table->capacity > SIZE_MAX / 2) {
        return false;
    }
    larger = ht_create(table->capacity * 2);
    if (larger == NULL) {
        return false;
    }
    /* A new mask changes bucket positions, so rehash every occupied entry. */
    for (i = 0; i < table->capacity; i++) {
        if (table->occupied[i]) {
            insert_without_resize(larger, table->keys[i], table->values[i]);
        }
    }
    free(table->keys);
    free(table->values);
    free(table->occupied);
    *table = *larger; /* Transfer ownership of the three new arrays. */
    free(larger);
    return true;
}

bool ht_put(HashTable *table, uint64_t key, uint64_t value)
{
    size_t slot;
    size_t load_limit;

    if (table == NULL) {
        return false;
    }
    slot = find_slot(table, key);
    if (table->occupied[slot]) {
        table->values[slot] = value;
        return true; /* Updating must not resize or increase the row count. */
    }
    /* floor(capacity * 7 / 10), written without an overflowing product. */
    load_limit = (table->capacity / 10) * 7 + (table->capacity % 10) * 7 / 10;
    if (table->size >= load_limit && !grow(table)) {
        return false;
    }
    insert_without_resize(table, key, value);
    return true;
}

bool ht_get(const HashTable *table, uint64_t key, uint64_t *out)
{
    size_t slot;
    if (table == NULL) {
        return false;
    }
    slot = find_slot(table, key);
    if (!table->occupied[slot]) {
        return false;
    }
    if (out != NULL) {
        *out = table->values[slot];
    }
    return true;
}

void ht_free(HashTable *table)
{
    if (table != NULL) {
        free(table->keys);
        free(table->values);
        free(table->occupied);
        free(table);
    }
}
