#include "test.h"
#include "ops.h"
#include "prng.h"

static void column_types_and_cleanup(void)
{
    Column a = {0}, b = {0};
    CHECK(column_create(&a, COLUMN_INT32, 4));
    CHECK(column_create(&b, COLUMN_INT64, 4));
    CHECK(a.length == 4 && b.length == 4 && a.data.i32[0] == 0 && b.data.i64[3] == 0);
    a.data.i32[0] = INT32_MIN;
    b.data.i64[0] = INT64_MAX;
    CHECK(column_valid(&a) && column_valid(&b));
    column_free(&a);
    column_free(&b);
    CHECK(a.length == 0 && a.data.i32 == NULL);
    column_free(&a);
    column_free(NULL);
}

static void allocation_input_validation(void)
{
    Column column = {0};
    SelectionVector selection = {0};
    CHECK(!column_create(NULL, COLUMN_INT32, 1));
    CHECK(!column_create(&column, (ColumnType)99, 1));
    CHECK(!column_create(&column, COLUMN_INT64, SIZE_MAX));
    CHECK(!selection_create(&selection, SIZE_MAX));
    CHECK(!selection_create(NULL, 1));
    CHECK(!column_valid(NULL));
    selection_free(NULL);
}

static void empty_columns(void)
{
    Column column = {0};
    SelectionVector selection = {0};
    HashTable *groups = NULL;
    int64_t sum = 9;
    CHECK(column_create(&column, COLUMN_INT64, 0));
    CHECK(selection_create(&selection, 0));
    CHECK(ops_filter_gt(&column, 0, &selection) == OPS_OK && selection.count == 0);
    CHECK(ops_sum(&column, NULL, &sum) == OPS_OK && sum == 0);
    CHECK(ops_group_sum(&column, &column, NULL, &groups) == OPS_OK && groups->size == 0);
    ht_free(groups);
    selection_free(&selection);
    column_free(&column);
}

static void filter_no_matches(void)
{
    int32_t values[] = {1, 2, 3};
    Column column = {COLUMN_INT32, 3, {.i32 = values}};
    SelectionVector selected = {0};
    CHECK(selection_create(&selected, 3));
    CHECK(ops_filter_gt(&column, 3, &selected) == OPS_OK && selected.count == 0);
    selection_free(&selected);
}

static void filter_all_matches(void)
{
    int64_t values[] = {-3, 0, INT64_MAX};
    Column column = {COLUMN_INT64, 3, {.i64 = values}};
    SelectionVector selected = {0};
    CHECK(selection_create(&selected, 3));
    CHECK(ops_filter_gt(&column, -4, &selected) == OPS_OK && selected.count == 3);
    CHECK(selected.rows[0] == 0 && selected.rows[2] == 2);
    selection_free(&selected);
}

static void filter_partial_strict_comparison(void)
{
    int32_t values[] = {-1, 10, 11, 20, 0};
    Column column = {COLUMN_INT32, 5, {.i32 = values}};
    SelectionVector selected = {0};
    CHECK(selection_create(&selected, 5));
    CHECK(ops_filter_gt(&column, 10, &selected) == OPS_OK && selected.count == 2);
    CHECK(selected.rows[0] == 2 && selected.rows[1] == 3);
    selection_free(&selected);
}

static void filter_resets_previous_selection(void)
{
    int64_t values[] = {1, 2, 3};
    Column column = {COLUMN_INT64, 3, {.i64 = values}};
    SelectionVector selected = {0};
    CHECK(selection_create(&selected, 3));
    CHECK(ops_filter_gt(&column, 0, &selected) == OPS_OK && selected.count == 3);
    CHECK(ops_filter_gt(&column, 9, &selected) == OPS_OK && selected.count == 0);
    selection_free(&selected);
}

static void sum_without_selection(void)
{
    int32_t values[] = {INT32_MAX, INT32_MAX, -1};
    Column column = {COLUMN_INT32, 3, {.i32 = values}};
    int64_t sum;
    CHECK(ops_sum(&column, NULL, &sum) == OPS_OK);
    CHECK(sum == INT64_C(4294967293)); /* int32 values accumulate as int64. */
}

static void sum_with_selection(void)
{
    int64_t values[] = {-10, 20, 30, 40};
    size_t rows[] = {3, 0, 1};
    Column column = {COLUMN_INT64, 4, {.i64 = values}};
    SelectionVector selected = {rows, 3, 3};
    int64_t sum;
    CHECK(ops_sum(&column, &selected, &sum) == OPS_OK && sum == 50);
}

static void empty_selection_sum(void)
{
    int64_t values[] = {INT64_MAX};
    Column column = {COLUMN_INT64, 1, {.i64 = values}};
    SelectionVector selected = {0};
    int64_t sum = -1;
    CHECK(ops_sum(&column, &selected, &sum) == OPS_OK && sum == 0);
}

static void duplicate_selection_rows(void)
{
    int64_t values[] = {7};
    size_t rows[] = {0, 0};
    Column column = {COLUMN_INT64, 1, {.i64 = values}};
    SelectionVector selected = {rows, 2, 2};
    int64_t sum;
    CHECK(ops_sum(&column, &selected, &sum) == OPS_OK && sum == 14);
}

static void positive_sum_overflow(void)
{
    int64_t values[] = {INT64_MAX, 1};
    Column column = {COLUMN_INT64, 2, {.i64 = values}};
    int64_t sum = 42;
    CHECK(ops_sum(&column, NULL, &sum) == OPS_OVERFLOW && sum == 42);
}

static void negative_sum_overflow(void)
{
    int64_t values[] = {INT64_MIN, -1};
    Column column = {COLUMN_INT64, 2, {.i64 = values}};
    int64_t sum = 42;
    CHECK(ops_sum(&column, NULL, &sum) == OPS_OVERFLOW && sum == 42);
}

static void selection_bounds_and_capacity(void)
{
    int32_t values[] = {1, 2};
    size_t rows[] = {2};
    Column column = {COLUMN_INT32, 2, {.i32 = values}};
    SelectionVector selected = {rows, 1, 1};
    int64_t sum = 42;
    HashTable *groups = NULL;
    CHECK(ops_filter_gt(&column, 0, &selected) == OPS_INVALID);
    CHECK(ops_sum(&column, &selected, &sum) == OPS_INVALID && sum == 42);
    CHECK(ops_group_sum(&column, &column, &selected, &groups) == OPS_INVALID && groups == NULL);
    selected.count = 2;
    CHECK(ops_sum(&column, &selected, &sum) == OPS_INVALID);
}

static void invalid_operator_inputs(void)
{
    Column malformed = {COLUMN_INT64, 3, {.i64 = NULL}};
    Column wrong_type = {(ColumnType)99, 0, {.i32 = NULL}};
    SelectionVector selected = {0};
    int64_t sum;
    CHECK(ops_filter_gt(&malformed, 0, &selected) == OPS_INVALID);
    CHECK(ops_filter_gt(&wrong_type, 0, &selected) == OPS_INVALID);
    CHECK(ops_sum(NULL, NULL, &sum) == OPS_INVALID);
    CHECK(ops_sum(&wrong_type, NULL, &sum) == OPS_INVALID);
    CHECK(ops_sum(&malformed, NULL, NULL) == OPS_INVALID);
}

static void vector_boundary_and_tail(void)
{
    /* Two full vectors plus a tail; matches lie on both sides of boundaries. */
    Column column = {0};
    SelectionVector selected = {0};
    const size_t length = VECTOR_SIZE * 2 + 3;
    size_t i, expected_count = 0;
    int64_t expected_sum = 0, sum;
    CHECK(column_create(&column, COLUMN_INT64, length));
    CHECK(selection_create(&selected, length));
    for (i = 0; i < length; i++) {
        column.data.i64[i] = (int64_t)(i % 11) - 5;
        if (column.data.i64[i] > 0) {
            expected_count++;
            expected_sum += column.data.i64[i];
        }
    }
    CHECK(ops_filter_gt(&column, 0, &selected) == OPS_OK);
    CHECK(selected.count == expected_count);
    CHECK(ops_sum(&column, &selected, &sum) == OPS_OK && sum == expected_sum);
    selection_free(&selected);
    column_free(&column);
}

static void group_signed_keys_and_values(void)
{
    int32_t keys[] = {0, -1, 0, -1, 2};
    int64_t values[] = {-5, 10, 3, -20, 0};
    Column key_column = {COLUMN_INT32, 5, {.i32 = keys}};
    Column value_column = {COLUMN_INT64, 5, {.i64 = values}};
    HashTable *groups = NULL;
    int64_t sum;
    CHECK(ops_group_sum(&key_column, &value_column, NULL, &groups) == OPS_OK);
    CHECK(groups->size == 3);
    CHECK(ops_group_get(groups, 0, &sum) && sum == -2);
    CHECK(ops_group_get(groups, -1, &sum) && sum == -10);
    CHECK(ops_group_get(groups, 2, &sum) && sum == 0);
    CHECK(!ops_group_get(groups, 999, &sum));
    ht_free(groups);
}

static void group_int64_keys_int32_values(void)
{
    int64_t keys[] = {INT64_MIN, INT64_MAX, INT64_MIN};
    int32_t values[] = {5, -7, 8};
    Column key_column = {COLUMN_INT64, 3, {.i64 = keys}};
    Column value_column = {COLUMN_INT32, 3, {.i32 = values}};
    HashTable *groups = NULL;
    int64_t sum;
    CHECK(ops_group_sum(&key_column, &value_column, NULL, &groups) == OPS_OK);
    CHECK(ops_group_get(groups, INT64_MIN, &sum) && sum == 13);
    CHECK(ops_group_get(groups, INT64_MAX, &sum) && sum == -7);
    ht_free(groups);
}

static void group_length_mismatch(void)
{
    int32_t keys[] = {1, 2}, values[] = {10};
    Column key_column = {COLUMN_INT32, 2, {.i32 = keys}};
    Column value_column = {COLUMN_INT32, 1, {.i32 = values}};
    HashTable *groups = NULL;
    CHECK(ops_group_sum(&key_column, &value_column, NULL, &groups) == OPS_INVALID);
    CHECK(groups == NULL);
}

static void group_overflow_cleans_up(void)
{
    int32_t keys[] = {1, 1};
    int64_t values[] = {INT64_MAX, 1};
    Column key_column = {COLUMN_INT32, 2, {.i32 = keys}};
    Column value_column = {COLUMN_INT64, 2, {.i64 = values}};
    HashTable *groups = NULL;
    CHECK(ops_group_sum(&key_column, &value_column, NULL, &groups) == OPS_OVERFLOW);
    CHECK(groups == NULL);
}

static void group_randomized_against_naive(void)
{
    const size_t length = 4099;
    Column keys = {0}, values = {0};
    SelectionVector selected = {0};
    HashTable *groups = NULL;
    uint64_t seed = 123456;
    size_t i;
    int64_t key;
    CHECK(column_create(&keys, COLUMN_INT32, length));
    CHECK(column_create(&values, COLUMN_INT64, length));
    CHECK(selection_create(&selected, length));
    for (i = 0; i < length; i++) {
        keys.data.i32[i] = (int32_t)(prng_next(&seed) % 23) - 11;
        values.data.i64[i] = (int64_t)(prng_next(&seed) % 2001) - 1000;
    }
    CHECK(ops_filter_gt(&values, -100, &selected) == OPS_OK);
    CHECK(ops_group_sum(&keys, &values, &selected, &groups) == OPS_OK);
    /* Independent reference scans the original arrays, without the selection
     * vector or hash table. This also checks filter composition. */
    for (key = -11; key <= 11; key++) {
        int64_t expected = 0, actual;
        bool present = false;
        for (i = 0; i < length; i++) {
            if (keys.data.i32[i] == key && values.data.i64[i] > -100) {
                expected += values.data.i64[i];
                present = true;
            }
        }
        CHECK(ops_group_get(groups, key, &actual) == present);
        CHECK(!present || actual == expected);
    }
    ht_free(groups);
    selection_free(&selected);
    column_free(&values);
    column_free(&keys);
}

unsigned run_ops_tests(void)
{
    unsigned tests_run = 0;
    RUN(column_types_and_cleanup);
    RUN(allocation_input_validation);
    RUN(empty_columns);
    RUN(filter_no_matches);
    RUN(filter_all_matches);
    RUN(filter_partial_strict_comparison);
    RUN(filter_resets_previous_selection);
    RUN(sum_without_selection);
    RUN(sum_with_selection);
    RUN(empty_selection_sum);
    RUN(duplicate_selection_rows);
    RUN(positive_sum_overflow);
    RUN(negative_sum_overflow);
    RUN(selection_bounds_and_capacity);
    RUN(invalid_operator_inputs);
    RUN(vector_boundary_and_tail);
    RUN(group_signed_keys_and_values);
    RUN(group_int64_keys_int32_values);
    RUN(group_length_mismatch);
    RUN(group_overflow_cleans_up);
    RUN(group_randomized_against_naive);
    return tests_run;
}
