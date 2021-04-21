#!/bin/bash
LDFLAGS="-fsanitize=${2-address} -fno-omit-frame-pointer" \
	CFLAGS="-Og -ggdb -fsanitize=${2-address} -fno-omit-frame-pointer" \
	VALGRIND="sh -c" \
	CC=${1-gcc} make test
LDFLAGS="-fsanitize=${2-address} -fno-omit-frame-pointer" \
	CFLAGS="-Og -ggdb -fsanitize=${2-address} -fno-omit-frame-pointer" \
	VALGRIND="sh -c" \
	CC=${1-gcc} make
