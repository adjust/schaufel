#!/bin/sh
CFLAGS='-Wall -std=c11 -Wextra -Werror -pedantic -Og -ggdb'
make clean && make test && make
