PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1
MAN5DIR = $(MANDIR)/man5
DOCDIR ?= $(PREFIX)/share/doc/schaufel
INSTALL ?= install -D

CC ?= gcc
LD = $(CC)
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -std=c11
CFLAGS += -D_XOPEN_SOURCE=700
CFLAGS += -D_SCHAUFEL_VERSION='"$(SCHAUFEL_VERSION)"'
CFLAGS += -D_GNU_SOURCE
LIB = -lpthread -lhiredis -lrdkafka -lpq -lconfig -ljson-c
INC = -Isrc/

OBJDIR = obj
OUT = bin/schaufel

SOURCES = $(wildcard src/*.c) $(wildcard src/utils/*.c)
TEST_SOURCES = $(wildcard t/*.c)

OBJ = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SOURCES))
OBJ_TEST = $(patsubst $(OBJDIR)/main.o, ,$(OBJ))
OBJ_BIN_TEST = $(patsubst t/%.c, $(OBJDIR)/%.o, $(TEST_SOURCES))

DOCS = $(patsubst man/%, doc/%.pdf , $(wildcard man/*))

SCHAUFEL_VERSION ?= 0.6

all: release

docs: $(DOCS)

doc/%.pdf: man/*
	groff -mandoc -f H -T ps $^ | ps2pdf - $@

release: before_release $(OBJ) out_release

test: clean_release before_release $(OBJ_TEST) $(OBJ_BIN_TEST)

before_release:
	mkdir -p obj/utils bin

clean: clean_release

clean_release:
	rm -f $(OBJ) $(OUT)
	rm -rf bin
	rm -rf $(OBJDIR)
	rm -rf doc/*.pdf

out_release: $(OBJ)
	$(LD) $(LIBDIR) $(OBJ) $(LIB) -o $(OUT)

$(OBJDIR)/%.o: src/%.c
	$(CC) $(INC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: t/%.c
	$(CC) $(INC) $(CFLAGS) -c $< -o $@
	$(LD) $(LIBDIR) $(OBJ_TEST) $@ $(LIB) -o bin/$(subst .o, ,$(notdir $@))
	valgrind -q --leak-check=full bin/$(subst .o, ,$(notdir $@))

install: all
	$(INSTALL) bin/schaufel $(DESTDIR)$(BINDIR)/schaufel
	$(INSTALL) -m 0644 -t $(DESTDIR)$(DOCDIR) doc/*
	$(INSTALL) -m 0644 man/schaufel.1 $(DESTDIR)$(MAN1DIR)/schaufel.1
	$(INSTALL) -m 0644 man/schaufel.conf.5 $(DESTDIR)$(MAN5DIR)/schaufel.conf.5
