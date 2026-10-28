#ifndef COLUMN_H
#define COLUMN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { COLUMN_INT32, COLUMN_INT64 } ColumnType;

typedef struct {
    ColumnType type;
    size_t length;
    union {
        int32_t *i32;
        int64_t *i64;
    } data;
} Column;

/* Create into an unowned output. Free before reusing it. Zero rows are valid.
 * Failure leaves the output unchanged. Values are initialized to zero. */
bool column_create(Column *out, ColumnType type, size_t length);
void column_free(Column *column);
bool column_valid(const Column *column);

#endif
