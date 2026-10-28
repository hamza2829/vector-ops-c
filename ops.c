#include "ops.h"

#include <stdlib.h>
#include <string.h>

bool selection_create(SelectionVector *out, size_t capacity)
{
    SelectionVector result = {0};
    if (out == NULL || capacity > SIZE_MAX / sizeof(*result.rows)) {
        return false;
    }
    if (capacity != 0) {
        result.rows = malloc(capacity * sizeof(*result.rows));
        if (result.rows == NULL) {
            return false;
        }
    }
    result.capacity = capacity;
    *out = result;
    return true;
}

void selection_free(SelectionVector *selection)
{
    if (selection != NULL) {
        free(selection->rows);
        *selection = (SelectionVector){0};
    }
}

static bool selection_valid(const SelectionVector *selection)
{
    return selection == NULL ||
        (selection->count <= selection->capacity &&
         (selection->count == 0 || selection->rows != NULL));
}

static int64_t value_at(const Column *column, size_t row)
{
    return column->type == COLUMN_INT32 ? column->data.i32[row] : column->data.i64[row];
}

static bool checked_add(int64_t left, int64_t right, int64_t *out)
{
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) {
        return false;
    }
    *out = left + right; /* Addition is safe only after these checks. */
    return true;
}

static size_t batch_end(size_t base, size_t length)
{
    size_t remaining = length - base;
    return base + (remaining < VECTOR_SIZE ? remaining : VECTOR_SIZE);
}

OpsStatus ops_filter_gt(const Column *column, int64_t constant, SelectionVector *out)
{
    size_t base;
    if (!column_valid(column) || out == NULL || out->capacity < column->length ||
        (column->length != 0 && out->rows == NULL)) {
        return OPS_INVALID;
    }
    out->count = 0;
    for (base = 0; base < column->length;) {
        size_t end = batch_end(base, column->length);
        size_t row;
        /* A batch contains up to 1024 contiguous values. The scalar inner
         * loop is deliberate: vector-at-a-time does not mean explicit SIMD. */
        for (row = base; row < end; row++) {
            if (value_at(column, row) > constant) {
                out->rows[out->count++] = row;
            }
        }
        base = end;
    }
    return OPS_OK;
}

OpsStatus ops_sum(const Column *column, const SelectionVector *selection, int64_t *out)
{
    size_t length, base;
    int64_t total = 0;
    if (!column_valid(column) || !selection_valid(selection) || out == NULL) {
        return OPS_INVALID;
    }
    length = selection == NULL ? column->length : selection->count;
    for (base = 0; base < length;) {
        size_t end = batch_end(base, length);
        size_t i;
        for (i = base; i < end; i++) {
            size_t row = selection == NULL ? i : selection->rows[i];
            if (row >= column->length) {
                return OPS_INVALID;
            }
            if (!checked_add(total, value_at(column, row), &total)) {
                return OPS_OVERFLOW;
            }
        }
        base = end;
    }
    *out = total;
    return OPS_OK;
}

static uint64_t encode_sum(int64_t value)
{
    uint64_t bits;
    /* memcpy copies the representation without aliasing or a lossy cast. */
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int64_t decode_sum(uint64_t bits)
{
    int64_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

OpsStatus ops_group_sum(const Column *keys, const Column *values,
                        const SelectionVector *selection, HashTable **out)
{
    HashTable *groups;
    size_t length, base;
    if (!column_valid(keys) || !column_valid(values) || keys->length != values->length ||
        !selection_valid(selection) || out == NULL) {
        return OPS_INVALID;
    }
    groups = ht_create(16);
    if (groups == NULL) {
        return OPS_ALLOCATION_FAILED;
    }
    length = selection == NULL ? keys->length : selection->count;
    for (base = 0; base < length;) {
        size_t end = batch_end(base, length);
        size_t i;
        for (i = base; i < end; i++) {
            size_t row = selection == NULL ? i : selection->rows[i];
            uint64_t key, bits;
            int64_t previous = 0, total;
            if (row >= keys->length) {
                ht_free(groups);
                return OPS_INVALID;
            }
            /* Signed-to-unsigned conversion is defined modulo 2^64. */
            key = (uint64_t)value_at(keys, row);
            if (ht_get(groups, key, &bits)) {
                previous = decode_sum(bits);
            }
            if (!checked_add(previous, value_at(values, row), &total)) {
                ht_free(groups);
                return OPS_OVERFLOW;
            }
            if (!ht_put(groups, key, encode_sum(total))) {
                ht_free(groups);
                return OPS_ALLOCATION_FAILED;
            }
        }
        base = end;
    }
    *out = groups;
    return OPS_OK;
}

bool ops_group_get(const HashTable *groups, int64_t key, int64_t *out)
{
    uint64_t bits;
    if (!ht_get(groups, (uint64_t)key, &bits)) {
        return false;
    }
    if (out != NULL) {
        *out = decode_sum(bits);
    }
    return true;
}
