/* clock_gettime is POSIX; the engine itself uses only C11. */
#define _POSIX_C_SOURCE 200809L

#include "ops.h"
#include "prng.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define REPETITIONS 7
#define HASH_ITEMS 200000
#define COLUMN_ROWS 5000000

#if defined(__clang__)
#define COMPILER "clang"
#elif defined(__GNUC__)
#define COMPILER "gcc"
#else
#define COMPILER "unknown"
#endif

typedef struct {
    const char *name;
    size_t items;
    double median_seconds;
    uint64_t checksum;
} Measurement;

/* Observable results keep the work live. There is one sink update per trial,
 * outside the timed section, not a volatile access for every row. */
static volatile uint64_t benchmark_sink;

static void fail(const char *message)
{
    fprintf(stderr, "Benchmark error: %s\n", message);
    exit(EXIT_FAILURE);
}

static double now_seconds(void)
{
    struct timespec timestamp;
    if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (double)timestamp.tv_sec + (double)timestamp.tv_nsec / 1e9;
}

static int compare_double(const void *a, const void *b)
{
    double left = *(const double *)a;
    double right = *(const double *)b;
    return (left > right) - (left < right);
}

static double median(double samples[REPETITIONS])
{
    qsort(samples, REPETITIONS, sizeof(*samples), compare_double);
    if (samples[REPETITIONS / 2] <= 0) {
        fail("timer resolution was insufficient");
    }
    return samples[REPETITIONS / 2];
}

static void benchmark_hash(Measurement *put, Measurement *get)
{
    HashTable *table = NULL;
    double samples[REPETITIONS];
    uint64_t *keys = malloc(HASH_ITEMS * sizeof(*keys));
    const uint64_t value_mask = UINT64_C(0xa5a5a5a5a5a5a5a5);
    uint64_t expected = 0, checksum = 0;
    size_t i;
    unsigned trial;
    if (keys == NULL) {
        fail("key allocation failed");
    }
    for (i = 0; i < HASH_ITEMS; i++) {
        /* Odd multiplication is a permutation modulo 2^64: no duplicate keys. */
        keys[i] = (uint64_t)i * UINT64_C(0x9e3779b97f4a7c15);
        expected += keys[i] ^ value_mask;
    }
    /* Trial zero warms up; the next seven trials form the median. Creation,
     * key generation and freeing are outside timing; growth during put is in. */
    for (trial = 0; trial <= REPETITIONS; trial++) {
        double start, elapsed;
        ht_free(table);
        table = ht_create(16);
        if (table == NULL) {
            fail("table allocation failed");
        }
        start = now_seconds();
        for (i = 0; i < HASH_ITEMS; i++) {
            if (!ht_put(table, keys[i], keys[i] ^ value_mask)) {
                fail("hash insertion failed");
            }
        }
        elapsed = now_seconds() - start;
        benchmark_sink ^= (uint64_t)table->size;
        if (trial != 0) {
            samples[trial - 1] = elapsed;
        }
    }
    *put = (Measurement){"hash_put_growing", HASH_ITEMS, median(samples), (uint64_t)table->size};
    for (trial = 0; trial <= REPETITIONS; trial++) {
        double start, elapsed;
        checksum = 0;
        start = now_seconds();
        for (i = 0; i < HASH_ITEMS; i++) {
            uint64_t value;
            if (!ht_get(table, keys[i], &value)) {
                fail("hash lookup unexpectedly missed");
            }
            checksum += value; /* Unsigned checksum overflow is defined. */
        }
        elapsed = now_seconds() - start;
        if (checksum != expected) {
            fail("hash lookup checksum mismatch");
        }
        benchmark_sink ^= checksum;
        if (trial != 0) {
            samples[trial - 1] = elapsed;
        }
    }
    *get = (Measurement){"hash_get_hit", HASH_ITEMS, median(samples), checksum};
    ht_free(table);
    free(keys);
}

static int64_t naive_filter_sum(const int64_t *values, size_t length, int64_t threshold)
{
    size_t row;
    int64_t total = 0;
    /* This benchmark's 0..999 inputs and five million rows provably fit int64.
     * It fuses filter and sum without producing row ids or a second pass. */
    for (row = 0; row < length; row++) {
        if (values[row] > threshold) {
            total += values[row];
        }
    }
    return total;
}

/* One indirect call per whole column prevents the compiler from hoisting a
 * pure naive loop out of repeated timing intervals. Inner loops stay normal. */
static int64_t (*volatile naive_query)(const int64_t *, size_t, int64_t) = naive_filter_sum;

static void benchmark_columns(Measurement *vector, Measurement *naive)
{
    Column column = {0};
    SelectionVector selected = {0};
    const int64_t threshold = 500;
    uint64_t seed = 42;
    int64_t expected, sum = 0;
    double samples[REPETITIONS];
    size_t i;
    unsigned trial;
    if (!column_create(&column, COLUMN_INT64, COLUMN_ROWS) ||
        !selection_create(&selected, COLUMN_ROWS)) {
        fail("column allocation failed");
    }
    for (i = 0; i < column.length; i++) {
        column.data.i64[i] = (int64_t)(prng_next(&seed) % 1000);
    }
    expected = naive_query(column.data.i64, column.length, threshold);
    for (trial = 0; trial <= REPETITIONS; trial++) {
        double start = now_seconds();
        double elapsed;
        if (ops_filter_gt(&column, threshold, &selected) != OPS_OK ||
            ops_sum(&column, &selected, &sum) != OPS_OK) {
            fail("vector query failed");
        }
        elapsed = now_seconds() - start;
        if (sum != expected) {
            fail("vector/naive result mismatch");
        }
        benchmark_sink ^= (uint64_t)sum;
        if (trial != 0) {
            samples[trial - 1] = elapsed;
        }
    }
    *vector = (Measurement){"filter_sum_vector_1024", COLUMN_ROWS, median(samples), (uint64_t)sum};
    for (trial = 0; trial <= REPETITIONS; trial++) {
        double start = now_seconds();
        double elapsed;
        sum = naive_query(column.data.i64, column.length, threshold);
        elapsed = now_seconds() - start;
        if (sum != expected) {
            fail("naive result mismatch");
        }
        benchmark_sink ^= (uint64_t)sum;
        if (trial != 0) {
            samples[trial - 1] = elapsed;
        }
    }
    *naive = (Measurement){"filter_sum_naive_fused", COLUMN_ROWS, median(samples), (uint64_t)sum};
    printf("Column rows=%d selected=%zu threshold=%" PRId64 " vector_size=%d\n",
           COLUMN_ROWS, selected.count, threshold, VECTOR_SIZE);
    selection_free(&selected);
    column_free(&column);
}

int main(int argc, char **argv)
{
    Measurement results[4];
    const char *path;
    FILE *csv;
    size_t i;
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [output.csv]\n", argv[0]);
        return EXIT_FAILURE;
    }
    path = argc == 2 ? argv[1] : "build/benchmark.csv";
    benchmark_hash(&results[0], &results[1]);
    benchmark_columns(&results[2], &results[3]);
    csv = fopen(path, "w");
    if (csv == NULL) {
        perror(path);
        return EXIT_FAILURE;
    }
    fprintf(csv, "compiler,metric,items,repetitions,median_seconds,items_per_second,checksum\n");
    printf("%s | one warm-up + %d measured repetitions | shared-runner timing is noisy\n",
           COMPILER, REPETITIONS);
    printf("%-25s %12s %16s\n", "metric", "median ms", "items/second");
    for (i = 0; i < 4; i++) {
        const Measurement *m = &results[i];
        double rate = (double)m->items / m->median_seconds;
        fprintf(csv, "%s,%s,%zu,%d,%.9f,%.2f,%" PRIu64 "\n",
                COMPILER, m->name, m->items, REPETITIONS, m->median_seconds, rate, m->checksum);
        printf("%-25s %12.3f %16.0f\n", m->name, m->median_seconds * 1000, rate);
    }
    if (fclose(csv) != 0) {
        perror("fclose");
        return EXIT_FAILURE;
    }
    printf("CSV: %s | sink=%" PRIu64 "\n", path, benchmark_sink);
    return EXIT_SUCCESS;
}
