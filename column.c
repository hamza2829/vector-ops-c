#include "column.h"

#include <stdlib.h>

bool column_create(Column *out, ColumnType type, size_t length)
{
    size_t width;
    void *data;
    Column result = {0};
    if (out == NULL || (type != COLUMN_INT32 && type != COLUMN_INT64)) {
        return false;
    }
    width = type == COLUMN_INT32 ? sizeof(int32_t) : sizeof(int64_t);
    if (length > SIZE_MAX / width) {
        return false;
    }
    data = length == 0 ? NULL : calloc(length, width);
    if (length != 0 && data == NULL) {
        return false;
    }
    result.type = type;
    result.length = length;
    if (type == COLUMN_INT32) {
        result.data.i32 = data;
    } else {
        result.data.i64 = data;
    }
    *out = result;
    return true;
}

bool column_valid(const Column *column)
{
    if (column == NULL) {
        return false;
    }
    if (column->type == COLUMN_INT32) {
        return column->length == 0 || column->data.i32 != NULL;
    }
    if (column->type == COLUMN_INT64) {
        return column->length == 0 || column->data.i64 != NULL;
    }
    return false;
}

void column_free(Column *column)
{
    if (column != NULL) {
        if (column->type == COLUMN_INT32) {
            free(column->data.i32);
        } else if (column->type == COLUMN_INT64) {
            free(column->data.i64);
        }
        *column = (Column){0};
    }
}
