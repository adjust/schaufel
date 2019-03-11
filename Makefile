PREFIX ?= /usr/local/bin
INSTALL ?= install

CC ?= gcc
LD = $(CC)
CFLAGS ?= -Wall -std=c11 -Wextra -Werror -pedantic -O2 -march=native
CFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -D_SCHAUFEL_VERSION='"$(SCHAUFEL_VERSION)"'
LIB = -lpthread -lhiredis -lrdkafka -lpq
INC = -Isrc/

OBJDIR = obj
OUT = bin/schaufel

SOURCES = $(wildcard src/*.c) $(wildcard src/utils/*.c)
TEST_SOURCES = $(wildcard t/*.c)

OBJ = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SOURCES))
OBJ_TEST = $(patsubst $(OBJDIR)/main.o, ,$(OBJ))
OBJ_BIN_TEST = $(patsubst t/%.c, $(OBJDIR)/%.o, $(TEST_SOURCES))

SCHAUFEL_VERSION ?= 0.5

all: release

release: before_release $(OBJ) out_release

test: clean_release before_release $(OBJ_TEST) $(OBJ_BIN_TEST)

before_release:
	mkdir -p obj/utils bin

clean: clean_release

clean_release:
	rm -f $(OBJ) $(OUT)
	rm -rf bin
	rm -rf $(OBJDIR)

out_release: $(OBJ)
	$(LD) $(LIBDIR) $(OBJ) $(LIB) -o $(OUT)

$(OBJDIR)/%.o: src/%.c
	$(CC) $(INC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: t/%.c
	$(CC) $(INC) $(CFLAGS) -c $< -o $@
	$(LD) $(LIBDIR) $(OBJ_TEST) $@ $(LIB) -o bin/$(subst .o, ,$(notdir $@))
	valgrind -q --leak-check=full bin/$(subst .o, ,$(notdir $@))

install: all
	${INSTALL} bin/schaufel ${PREFIX}/schaufel
