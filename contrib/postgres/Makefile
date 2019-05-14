CC ?= gcc
LD = $(CC)
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -std=c11
CFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -fPIC
CFLAGS += -lpq -lconfig -lpthread
CFLAGS += -I../../src/

OBJ = postgres.o

all: remove_obj link

link: $(OBJ)
	$(LD) $(OBJ) -shared -o postgres.so

remove_obj:
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all link remove_obj
