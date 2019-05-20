include ../../Makefile.global

CFLAGS += -fPIC
CFLAGS += -lhiredis -lconfig -lpthread
CFLAGS += -I../../src/

OBJ = redis.o
LIB = redis.so

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
