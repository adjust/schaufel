PREFIX=/usr/local/bin
INSTALL=install -c

CC = gcc
LD = gcc
CFLAGS = -Wall -std=c11 -Wextra -Werror -pedantic
CFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -D_SCHAUFEL_VERSION='"$(SCHAUFEL_VERSION)"'
LIB = -lpthread -lhiredis -lrdkafka -lpq
INC = -Isrc/

CFLAGS_DEBUG = $(CFLAGS) -g
LIB_DEBUG = $(LIB)
LIBDIR_DEBUG = $(LIBDIR)
INC_DEBUG = $(INC)
OBJDIR_DEBUG = obj/Debug
OUT_DEBUG = bin/Debug/schaufel

CFLAGS_RELEASE = $(CFLAGS) -O2 -march=native
LIB_RELEASE = $(LIB)
LIBDIR_RELEASE = $(LIBDIR)
INC_RELEASE = $(INC)
OBJDIR_RELEASE = obj/Release
OUT_RELEASE = bin/Release/schaufel

SOURCES = $(wildcard src/*.c) $(wildcard src/utils/*.c)
TEST_SOURCES = $(wildcard t/*.c)

OBJ_DEBUG = $(patsubst src/%.c, $(OBJDIR_DEBUG)/%.o, $(SOURCES))
OBJ_RELEASE = $(patsubst src/%.c, $(OBJDIR_RELEASE)/%.o, $(SOURCES))
OBJ_TEST = $(patsubst $(OBJDIR_DEBUG)/main.o, ,$(OBJ_DEBUG))
OBJ_BIN_TEST = $(patsubst t/%.c, $(OBJDIR_DEBUG)/%.o, $(TEST_SOURCES))

SCHAUFEL_VERSION = 0.4

all: debug release

debug: before_debug $(OBJ_DEBUG) out_debug

release: before_release $(OBJ_RELEASE) out_release

test: clean_debug before_debug $(OBJ_TEST) $(OBJ_BIN_TEST)

before_debug:
	test -d bin/Debug || mkdir -p bin/Debug
	test -d obj/Debug || mkdir -p obj/Debug
	test -d obj/Debug/utils || mkdir -p obj/Debug/utils

before_release:
	test -d bin/Release || mkdir -p bin/Release
	test -d obj/Release || mkdir -p obj/Release
	test -d obj/Release/utils || mkdir -p obj/Release/utils

clean: clean_debug clean_release

clean_debug:
	rm -f $(OBJ_DEBUG) $(OUT_DEBUG)
	rm -rf bin/Debug
	rm -rf $(OBJDIR_DEBUG)

clean_release:
	rm -f $(OBJ_RELEASE) $(OUT_RELEASE)
	rm -rf bin/Release
	rm -rf $(OBJDIR_RELEASE)

out_debug: $(OBJ_DEBUG)
	$(LD) $(LIBDIR_DEBUG) $(OBJ_DEBUG) $(LIB_DEBUG) -o $(OUT_DEBUG)

out_release: $(OBJ_RELEASE)
	$(LD) $(LIBDIR_RELEASE) $(OBJ_RELEASE) $(LIB_RELEASE) -o $(OUT_RELEASE)

$(OBJDIR_DEBUG)/%.o: src/%.c
	$(CC) $(INC_DEBUG) $(CFLAGS_DEBUG) -c $< -o $@

$(OBJDIR_RELEASE)/%.o: src/%.c
	$(CC) $(INC_RELEASE) $(CFLAGS_RELEASE) -c $< -o $@

$(OBJDIR_DEBUG)/%.o: t/%.c
	$(CC) $(INC_DEBUG) $(CFLAGS_DEBUG) -c $< -o $@
	$(LD) $(LIBDIR_DEBUG) $(OBJ_TEST) $@ $(LIB_DEBUG) -o bin/Debug/$(subst .o, ,$(notdir $@))
	valgrind -q --leak-check=full bin/Debug/$(subst .o, ,$(notdir $@))

install: all
	${INSTALL} bin/Debug/schaufel ${PREFIX}/schaufel
