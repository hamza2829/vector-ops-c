#include "test.h"
#include "hash_table.h"
#include "prng.h"

static void empty_table(void)
{
    HashTable *table = ht_create(0);
    uint64_t out = 99;
    CHECK(table != NULL && table->capacity == 8 && table->size == 0);
    CHECK(!ht_get(table, 123, &out) && out == 99);
    ht_free(table);
}

static void insert_and_lookup(void)
{
    HashTable *table = ht_create(8);
    uint64_t out = 0;
    CHECK(table != NULL && ht_put(table, 42, 1234));
    CHECK(ht_get(table, 42, &out) && out == 1234);
    CHECK(table->size == 1);
    ht_free(table);
}

static void update_existing(void)
{
    HashTable *table = ht_create(8);
    uint64_t out;
    CHECK(table != NULL && ht_put(table, 42, 1) && ht_put(table, 42, 2));
    CHECK(ht_get(table, 42, &out) && out == 2 && table->size == 1);
    ht_free(table);
}

static void missing_key(void)
{
    HashTable *table = ht_create(8);
    uint64_t out = 123;
    CHECK(table != NULL && ht_put(table, 1, 0));
    CHECK(!ht_get(table, 2, &out) && out == 123);
    CHECK(ht_get(table, 1, NULL));
    ht_free(table);
}

static void zero_key_and_value(void)
{
    HashTable *table = ht_create(8);
    uint64_t out = 999;
    CHECK(table != NULL && ht_put(table, 0, 0));
    CHECK(ht_get(table, 0, &out) && out == 0);
    CHECK(ht_put(table, 0, 123));
    CHECK(ht_get(table, 0, &out) && out == 123 && table->size == 1);
    ht_free(table);
}

static void maximum_key_and_value(void)
{
    HashTable *table = ht_create(8);
    uint64_t out;
    CHECK(table != NULL && ht_put(table, UINT64_MAX, UINT64_MAX));
    CHECK(ht_get(table, UINT64_MAX, &out) && out == UINT64_MAX);
    ht_free(table);
}

static void capacity_rounding(void)
{
    HashTable *table = ht_create(13);
    CHECK(table != NULL && table->capacity == 16);
    ht_free(table);
}

static void seventy_percent_growth(void)
{
    HashTable *table = ht_create(8);
    uint64_t i;
    CHECK(table != NULL);
    for (i = 0; i < 5; i++) {
        CHECK(ht_put(table, i, i + 10));
    }
    CHECK(table->capacity == 8);
    CHECK(ht_put(table, 1, 100) && table->capacity == 8);
    CHECK(ht_put(table, 5, 15) && table->capacity == 16 && table->size == 6);
    ht_free(table);
}

static void many_collisions_wrap_around(void)
{
    HashTable *table = ht_create(256);
    uint64_t keys[100];
    uint64_t candidate = 0;
    size_t i;
    CHECK(table != NULL);
    for (i = 0; i < 100; i++) {
        while ((ht_hash(candidate) & UINT64_C(255)) != UINT64_C(255)) {
            candidate++;
        }
        keys[i] = candidate++;
        CHECK(ht_put(table, keys[i], (uint64_t)i));
    }
    CHECK(table->capacity == 256 && table->occupied[255] && table->occupied[0]);
    for (i = 0; i < 100; i++) {
        uint64_t out;
        CHECK(ht_get(table, keys[i], &out) && out == (uint64_t)i);
    }
    CHECK(ht_put(table, keys[50], 999));
    CHECK(table->size == 100);
    CHECK(!ht_get(table, candidate, NULL));
    ht_free(table);
}

static void repeated_resizes_preserve_entries(void)
{
    HashTable *table = ht_create(8);
    uint64_t i;
    CHECK(table != NULL);
    for (i = 0; i < 10000; i++) {
        CHECK(ht_put(table, i * 3, i + 100));
        CHECK((table->capacity & (table->capacity - 1)) == 0);
    }
    CHECK(table->size == 10000);
    for (i = 0; i < 10000; i++) {
        uint64_t out;
        CHECK(ht_get(table, i * 3, &out) && out == i + 100);
    }
    ht_free(table);
}

static void null_and_impossible_capacity(void)
{
    CHECK(!ht_put(NULL, 0, 0));
    CHECK(!ht_get(NULL, 0, NULL));
    CHECK(ht_create(SIZE_MAX) == NULL);
    ht_free(NULL);
}

static void randomized_against_linear_reference(void)
{
    uint64_t keys[1000], values[1000];
    size_t count = 0, step;
    uint64_t seed = 42;
    HashTable *table = ht_create(8);
    CHECK(table != NULL);
    for (step = 0; step < 20000; step++) {
        uint64_t key = prng_next(&seed) % 1000;
        size_t i = 0;
        while (i < count && keys[i] != key) {
            i++;
        }
        if (prng_next(&seed) % 3 != 0) {
            uint64_t value = prng_next(&seed);
            CHECK(ht_put(table, key, value));
            if (i == count) {
                keys[count++] = key;
            }
            values[i] = value;
        } else {
            uint64_t out;
            bool found = ht_get(table, key, &out);
            CHECK(found == (i < count));
            if (found) {
                CHECK(out == values[i]);
            }
        }
    }
    CHECK(table->size == count);
    ht_free(table);
}

unsigned run_hash_tests(void)
{
    unsigned tests_run = 0;
    RUN(empty_table);
    RUN(insert_and_lookup);
    RUN(update_existing);
    RUN(missing_key);
    RUN(zero_key_and_value);
    RUN(maximum_key_and_value);
    RUN(capacity_rounding);
    RUN(seventy_percent_growth);
    RUN(many_collisions_wrap_around);
    RUN(repeated_resizes_preserve_entries);
    RUN(null_and_impossible_capacity);
    RUN(randomized_against_linear_reference);
    return tests_run;
}
