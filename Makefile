CC = gcc
CPPFLAGS = -I.
WARNINGS = -std=c11 -Wall -Wextra -Werror -Wpedantic
TEST_FLAGS = $(WARNINGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: build test bench clean
build: build/hash_table.o

build:

build/hash_table.o: hash_table.c hash_table.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(WARNINGS) -O2 -c hash_table.c -o $@

build/tests: hash_table.c tests/test_hash_table.c tests/test_main.c hash_table.h tests/test.h prng.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) hash_table.c tests/test_hash_table.c tests/test_main.c -o $@

test: build/tests
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/tests

bench:
	@echo "Benchmark target will be added after the operators."

clean:
	rm -rf build
