#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>

/* Fixed-seed xorshift64*: reproducible test data, not security randomness.
 * The caller supplies a nonzero state. Unsigned arithmetic wraps safely. */
static inline uint64_t prng_next(uint64_t *state)
{
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * UINT64_C(2685821657736338717);
}

#endif
