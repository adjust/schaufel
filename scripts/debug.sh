#!/bin/sh
CFLAGS='-Werror -Og -ggdb'
make test && make
