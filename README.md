Schaufel - shovel data around
=============================

Copyright (c) 2017, Robert Abraham

[https://github.com/adjust/schaufel](https://github.com/adjust/schaufel)

**schaufel** aims to be a swiss army knife for moving data. It can take
data from list-like sources and insert them into list-like sinks. In other
words, it can take data from a _redis_ list and insert it into _kafka_ or
vice versa.

_Producers_ and _consumers_ of all types can be run in parallel.

schaufel tries to be efficient but primitive. Its queueing can exceed
150000 messages/second but is upper bound by lock contention (as it works
on single elements instead of pipelining).

It compiles on linux, freebsd and netbsd, but work is in progress
to remove all gnuisms. It does compile with libmusl.

**schaufel** is licensed under the MIT license.

# Features #
  * kafka support (through librdkafka)
  * redis support (through hiredis)
  * file sink/source support
  * postgres sink support
  * json dereferencing

# Documentation
 * Introduction and manual in [INTRODUCTION.md](https://github.com/adjust/schaufel/blob/master/INTRODUCTION.md).
 * man pages _man 1 schaufel_ and _man 5 schaufel.conf_

# Installation

## Build from source

### Requirements

    gcc/clang
    GNU make
    pthreads
    librdkafka
    hiredis
    libpq (postgres)
    libjson-c
    a libc that supports hcreate_r/tdestroy

### Building
#### Makefile based build
    make
    make install

#### Autotools based build
    autoreconf -fi
    ./configure
    make

# Tests #
    * make test
        run testsuite (do not run in parallel if using old Makefile)
    * scripts/debug.sh
        run testsuite with Werror and debug symbols
    * scripts/sanitize.sh
        run sanitizer and valgrind testsuite
