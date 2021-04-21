#!/bin/sh
make clean
CFLAGS='-Werror -Og -ggdb' make test && make
