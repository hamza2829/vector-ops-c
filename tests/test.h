#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define RUN(test) do { \
    test(); \
    tests_run++; \
    printf("PASS %s\n", #test); \
} while (0)

unsigned run_hash_tests(void);
unsigned run_ops_tests(void);

#endif
