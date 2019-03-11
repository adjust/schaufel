#!/bin/sh
CFLAGS='-Wall -std=c11 -Wextra -Werror -pedantic -Og -ggdb'
make test && make
