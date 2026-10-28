# Recorded benchmark evidence

Date: 28 October 2026. Run: https://github.com/hamza2829/vector-ops-c/actions/runs/40608999935

Source commit: `5eeaa7295bf04c91bad0233d20e03df29a068525`.
Both jobs used GitHub-hosted ubuntu-latest (Ubuntu 24.04 toolchain) and -O2,
without sanitizers or architecture-specific compiler tuning for the benchmarks.

| Job | Compiler reported by runner | CPU model reported by lscpu |
| --- | --- | --- |
| gcc | gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 | AMD EPYC 9V45 96-Core Processor |
| clang | Ubuntu clang version 18.1.3 (1ubuntu1) | AMD EPYC 9V74 80-Core Processor |

Model names describe host processors, not an allocation of 96 or 80 cores to
these jobs. CPU frequency, host scheduling, contention and caches were not
controlled. The two jobs are not a controlled GCC-versus-Clang experiment.

Each CSV has four measurements, seven timed repetitions and one preceding
warm-up trial per measurement. Median throughput is input count divided by
median elapsed seconds. Both column implementations returned 1871702013.
Both hash-get implementations returned checksum 16700841773449111776.
Each test binary reported 33 tests passed with address/leak/UB checks active.
The example processed 10000000 rows, selected 4499530 and formed 1000 groups.

Raw CSVs were copied directly from the run artifacts, not transcribed from a
performance estimate. Complete environment, test, example and benchmark console
outputs are in the results-gcc/results-clang artifacts (30-day retention).
