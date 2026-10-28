# vector-ops-c

[![CI](https://github.com/hamza2829/vector-ops-c/actions/workflows/ci.yml/badge.svg)](https://github.com/hamza2829/vector-ops-c/actions/workflows/ci.yml)

A small C11 learning project showing the core ideas of a vectorized column
store. It is a query engine core, not a database. The example builds ten million
synthetic rows and evaluates the equivalent of:

```sql
SELECT key, SUM(value) FROM t WHERE value > 100 GROUP BY key;
```

There are no external C libraries, Docker images or test frameworks. Tests and
benchmarks run in GitHub Actions on Ubuntu with GCC and Clang. Windows users
can inspect the source and download the CSV artifacts without a C toolchain.

## Design in five files

| File | Responsibility |
| --- | --- |
| hash_table.c / .h | uint64 keys and values, linear probing, occupancy bytes, growth. |
| column.c / .h | Owned int32/int64 arrays with a type and length. |
| ops.c / .h | 1024-row filter, selected sum and hash group-by sum. |
| main.c | Ten-million-row synthetic query and the first eight group keys. |
| bench.c | Monotonic-clock timings, warm-up, seven repetitions, median CSV. |

`prng.h` is a tiny deterministic generator for reproducible synthetic inputs.
`tests/` uses a CHECK macro and plain C functions. The current suite has 33 tests.

```text
int32 key column -----------+
                           |
int64 value column -> filter (> X) -> selection of row ids
                           |               |
                           +------ group-by sum -> hash table
                                           |
                                      selected sum
```

## Hash table

`ht_create`, `ht_put`, `ht_get` and `ht_free` provide a deliberately small API.
`ht_hash` uses SplitMix64-style unsigned mixing. Capacity is a power of two with
a minimum of eight. The first bucket is hash(key) & (capacity - 1); collisions
advance one slot at a time, wrapping with the same mask.

Keys, values and occupancy bytes are three parallel contiguous arrays. Key zero
and UINT64_MAX are valid because no key is reserved as an empty marker. Slots
use 17 bytes each on an eight-bit-byte machine, excluding metadata/allocator
overhead and empty slots. A new insertion that would exceed floor(70% capacity)
doubles capacity and rehashes all entries. Updating an existing key changes only
its value. A failed allocation leaves the existing table usable.

Linear probing is often cache friendly because nearby probes visit nearby
occupancy/key slots without chasing separately allocated linked-list nodes.
It still suffers from clustering and long probe chains. This design uses a
conservative load factor and a mixing hash; it does not claim to beat modern
production hash tables. There is no deletion or tombstone handling.

## Columns and vector-at-a-time operators

A Column contains a type, a length and a union of typed array pointers. It owns
its allocation when created with column_create; tests may also construct borrowed
views of stack arrays. Only call column_free on owned columns. Allocation sizes
are checked before multiplication. An empty column is valid.

Each operator has an outer loop over up to **1024 values** and a normal scalar
inner loop. The final partial vector is handled explicitly. This means batching,
not explicit SIMD intrinsics or an assertion that the compiler emitted SIMD.

1. `ops_filter_gt` writes absolute row ids into a caller-owned SelectionVector.
   The comparison is strictly greater than the constant; source order is kept.
2. `ops_sum` accepts a selection or NULL for the whole column. It returns an
   int64 sum and checks signed overflow before each addition.
3. `ops_group_sum` takes key/value columns of equal length and an optional
   selection. It looks up each group, adds the value and upserts the aggregate.
   Both input types are supported, including negative keys and values.

The hash table stores uint64 values. Group sums use memcpy to preserve signed
int64 representations without pointer aliasing or an out-of-range cast. Read
them with ops_group_get. Signed keys convert to uint64 modulo 2^64, preserving
distinct keys. Operators return OPS_OK, OPS_INVALID, OPS_OVERFLOW or
OPS_ALLOCATION_FAILED. Sum outputs and group-result pointers are unchanged on
failure; failed groups free their temporary table. A zero-initialized output
must be unowned before create/group operations, and callers release ownership.
Duplicate selection row ids intentionally contribute more than once.

Vector-at-a-time designs can reduce per-row iterator calls, improve sequential
access and give a compiler longer loops to optimize. This small core has no
per-row iterator hierarchy to eliminate, and it materializes a whole-column
selection before summing. A fused row loop can therefore be faster for this
one simple query. The benchmark reports the measured outcome, not a promised
speedup. An automatic SIMD compiler transformation is allowed for either path.

## Build and test

The Makefile is for a Linux/POSIX environment. Nothing is compiled or benchmarked
on the Windows PC. Push a commit, open Actions, and download **results-gcc** and
**results-clang** from the completed run. Each includes the CSV, console table,
test/example output and compiler/CPU/runner metadata. The workflow can also be
started manually from GitHub's Actions tab.

These are the commands run on Ubuntu:

```sh
make build CC=gcc
./build/vector-ops
make test CC=gcc
make bench CC=gcc
make clean

make build CC=clang
make test CC=clang
make bench CC=clang
make clean
```

`build` compiles the example with C11, -Wall -Wextra -Werror -Wpedantic and -O2.
`test` uses -O0 -g, address/undefined behavior sanitizers, frame pointers,
detect_leaks=1 and halt_on_error=1. `bench` uses -O2 without sanitizers and writes
build/benchmark.csv. Clean before switching compilers in the same checkout.
The core uses C11; only the benchmark timer needs POSIX clock_gettime.

Tests cover missing and extreme keys, key zero, updates, 100 deliberately
colliding keys, wraparound, resize thresholds, repeated rehashes and a fixed-seed
20,000-operation comparison against a naive linear reference. Operator tests
cover no/all/partial matches, vector boundaries and tails, empty inputs,
selections, signed values, invalid bounds, overflow and random group-by against
an independent nested-loop reference. Both compiler jobs run the same 33 tests.

## Benchmarks

**Shared CI runner timing is noisy. These are measurements, not guarantees.**

Measured on 28 October 2026 in this [successful Actions run](https://github.com/hamza2829/vector-ops-c/actions/runs/40608999935)
at commit `5eeaa7295bf04c91bad0233d20e03df29a068525`. The checked-in
[GCC CSV](benchmarks/ci-gcc.csv), [Clang CSV](benchmarks/ci-clang.csv) and
[runner metadata](benchmarks/ENVIRONMENT.md) preserve the evidence after the
downloadable CI artifacts expire. No timings were measured on Windows.

<!-- BENCHMARK_TABLE_START -->
| Compiler | Metric | Median ms | Million input items/s |
| --- | --- | ---: | ---: |
| GCC 13.3 | Hash put, including growth | 10.827 | 18.47 |
| GCC 13.3 | Hash get, all hits | 1.367 | 146.31 |
| GCC 13.3 | Filter + sum, vectors of 1024 | 19.362 | 258.23 |
| GCC 13.3 | Filter + sum, fused row loop | 2.272 | 2200.61 |
| Clang 18.1 | Hash put, including growth | 16.720 | 11.96 |
| Clang 18.1 | Hash get, all hits | 4.361 | 45.86 |
| Clang 18.1 | Filter + sum, vectors of 1024 | 27.129 | 184.30 |
| Clang 18.1 | Filter + sum, fused row loop | 2.189 | 2283.80 |
<!-- BENCHMARK_TABLE_END -->

Items mean keys for hash operations and input rows for filter/sum. GCC ran on an
AMD EPYC 9V45 and Clang on an AMD EPYC 9V74; this is **not a controlled compiler
comparison**. The fused row loop won for this simple query. Selection-vector
materialization, a second scan and the general operator's safety checks have
real costs. The result supports batching as a query-engine building block,
not a claim that adding a 1024-row outer loop automatically improves speed.

Method:

- One untimed warm-up trial and seven measured trials per metric; report the
  median elapsed time using CLOCK_MONOTONIC. Allocation of the initial table,
  column arrays and random inputs is outside timing.
- Hash put inserts 200,000 unique keys from capacity 16; resizes are inside the
  timed region. Hash get measures successful lookups of all those keys in the
  resulting table, including result checksum accumulation.
- Filter/sum processes five million int64 values in 0..999 with value > 500,
  approximately 50% selected. The vector path materializes row ids in 1024-row
  batches and performs a second pass with bounds/overflow checks. The naive
  path fuses filter and sum in one pass; the bounded dataset makes overflow
  impossible without per-add checks. They implement the same logical query,
  with different materialization and checking costs. This is not an isolated
  experiment proving that a batch size of 1024 itself improves speed.
- Every measured query is checked against the naive result; hash lookups are
  checked against an expected checksum. A volatile sink consumes each result
  outside timing. A volatile function pointer prevents hoisting the naive
  loop out of repeated timing intervals, with one call per whole column.
- Throughput is input keys/second for hash metrics and input rows/second for
  column metrics. Group-by performance is not benchmarked.

## Limits and interview talking points

Real: actual allocations, collision handling, resize/rehash, selection vectors,
signed aggregation, overflow errors, independent reference tests, sanitizer
execution and measured CI timing. The ten-million-row example executes in CI.

Simplified: single threaded; only int32 and int64; no NULL values, SQL parser,
query optimizer, transactions, indexes, joins, disk storage, persistence or
concurrency. Selection vectors are fully materialized and operators do not
stream batches between stages. There are no SIMD intrinsics, compressed columns
or advanced hash-table control bytes. Memory-allocation failures are handled,
but exhaustive allocator fault injection is not part of this small suite.
Representable intermediate sums are required: an overflowing partial sum
returns OPS_OVERFLOW even if later values could bring the result back in range.
The benchmark numbers come from a shared CI runner with uncontrolled scheduling,
CPU frequency and cache state; GCC/Clang jobs may run on different CPU models.
The README table is a dated snapshot, not an automatically updated performance
promise. API callers must supply valid owned/borrowed buffers and must not
mutate hash-table internals. This is a learning core, not production software.
