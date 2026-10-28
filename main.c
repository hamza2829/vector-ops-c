#include "ops.h"
#include "prng.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const size_t rows = 10000000;
    const int64_t threshold = 100;
    Column keys = {0}, values = {0};
    SelectionVector selected = {0};
    HashTable *groups = NULL;
    uint64_t seed = 42;
    size_t i;
    int status = EXIT_FAILURE;

    if (!column_create(&keys, COLUMN_INT32, rows) ||
        !column_create(&values, COLUMN_INT64, rows) ||
        !selection_create(&selected, rows)) {
        fprintf(stderr, "Could not allocate the 10-million-row example\n");
        goto cleanup;
    }
    for (i = 0; i < rows; i++) {
        keys.data.i32[i] = (int32_t)(prng_next(&seed) % 1000);
        values.data.i64[i] = (int64_t)(prng_next(&seed) % 2001) - 1000;
    }
    if (ops_filter_gt(&values, threshold, &selected) != OPS_OK ||
        ops_group_sum(&keys, &values, &selected, &groups) != OPS_OK) {
        fprintf(stderr, "Query failed (invalid input, allocation or sum overflow)\n");
        goto cleanup;
    }
    printf("SELECT key, SUM(value) FROM t WHERE value > %" PRId64 " GROUP BY key\n", threshold);
    printf("rows=%zu selected=%zu groups=%zu vector_size=%d\n",
           rows, selected.count, groups->size, VECTOR_SIZE);
    printf("First eight group keys (synthetic, fixed seed):\n");
    for (i = 0; i < 8; i++) {
        int64_t sum;
        if (ops_group_get(groups, (int64_t)i, &sum)) {
            printf("key=%zu sum=%" PRId64 "\n", i, sum);
        }
    }
    status = EXIT_SUCCESS;
cleanup:
    ht_free(groups);
    selection_free(&selected);
    column_free(&values);
    column_free(&keys);
    return status;
}
