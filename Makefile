CC = gcc
CPPFLAGS = -I.
WARNINGS = -std=c11 -Wall -Wextra -Werror -Wpedantic
TEST_FLAGS = $(WARNINGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
CORE = hash_table.c column.c ops.c
HEADERS = hash_table.h column.h ops.h prng.h
TEST_SOURCES = tests/test_main.c tests/test_hash_table.c tests/test_ops.c

.PHONY: build test bench clean
build: build/vector-ops

build/vector-ops: main.c $(CORE) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(WARNINGS) -O2 main.c $(CORE) -o $@

build/tests: $(CORE) $(TEST_SOURCES) $(HEADERS) tests/test.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(CORE) $(TEST_SOURCES) -o $@

test: build/tests
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tests

build/bench: bench.c $(CORE) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(WARNINGS) -O2 bench.c $(CORE) -o $@

bench: build/bench
	./build/bench build/benchmark.csv

clean:
	rm -rf build
