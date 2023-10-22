.PHONY: run run-nofail check-avx512f clean

CC ?= gcc
CFLAGS ?= -Wall -Wextra -g -Og -fsanitize=address -fno-stack-protector -mavx512f

run: repro check-avx512f
	@echo '* This command is expected to segfault:'
	ASAN_OPTIONS=detect_stack_use_after_return=1 ./repro

run-nofail: repro check-avx512f
	@echo '* This command should not fail:'
	ASAN_OPTIONS=detect_stack_use_after_return=0 ./repro

check-avx512f:
	@echo '* Checking if this CPU supports AVX-512F...'
	grep -q avx512f /proc/cpuinfo

clean:
	${RM} repro
	${RM} a-repro.* a.out

a-repro.i: repro.c
	${CC} -v -save-temps ${CFLAGS} $^
	sed -i '/^#/d' $@
