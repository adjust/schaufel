include ../../Makefile.global

CFLAGS += -fPIC
CFLAGS += -lpq -lconfig -lpthread
CFLAGS += -I../../src/

OBJ = postgres.o
LIB = postgres.so

all: clean link

link: $(OBJ)
	$(LD) $(OBJ) -shared -o $(LIB)

clean:
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL) $(LIB) $(CONTRIBDIR)/$(LIB)

.PHONY: all link clean install
