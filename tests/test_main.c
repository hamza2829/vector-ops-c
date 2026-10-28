#include "test.h"

int main(void)
{
    unsigned count = run_hash_tests();
    count += run_ops_tests();
    printf("%u tests passed\n", count);
    return EXIT_SUCCESS;
}
