include ../../Makefile.global

CFLAGS += -fPIC
CFLAGS += -lpq -ljson-c -lconfig -lpthread
CFLAGS += -I../../src/

OBJ = exports.o
LIB = exports.so

all: clean link

link: $(OBJ)
	$(LD) $(OBJ) -shared -o $(LIB)

clean:
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL) $(LIB) $(DESTDIR)$(INSTALL_LIBDIR)/$(LIB)

.PHONY: all link clean install
