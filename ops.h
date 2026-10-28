#ifndef OPS_H
#define OPS_H

#include "column.h"
#include "hash_table.h"

#define VECTOR_SIZE 1024

typedef struct {
    size_t *rows;       /* Absolute row ids, in source order after filtering. */
    size_t count;
    size_t capacity;
} SelectionVector;

typedef enum { OPS_OK, OPS_INVALID, OPS_OVERFLOW, OPS_ALLOCATION_FAILED } OpsStatus;

bool selection_create(SelectionVector *out, size_t capacity);
void selection_free(SelectionVector *selection);

/* The output needs worst-case capacity >= column->length. Filtering replaces
 * count and writes matching row ids. It does not copy the column's values. */
OpsStatus ops_filter_gt(const Column *column, int64_t constant, SelectionVector *out);
/* NULL selection means every row. Otherwise sum the specified row ids;
 * duplicates intentionally contribute twice. Output is unchanged on failure. */
OpsStatus ops_sum(const Column *column, const SelectionVector *selection, int64_t *out);
/* Both columns must have equal lengths. Returns a newly owned hash table.
 * Failed operations free intermediate allocations and leave *out unchanged. */
OpsStatus ops_group_sum(const Column *keys, const Column *values,
                        const SelectionVector *selection, HashTable **out);
/* Group sums are signed int64_t encoded in uint64_t table values. Use this
 * accessor instead of an implementation-defined unsigned-to-signed cast. */
bool ops_group_get(const HashTable *groups, int64_t key, int64_t *out);

#endif
