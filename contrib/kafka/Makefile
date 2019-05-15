include ../../Makefile.global

CFLAGS += -fPIC
CFLAGS += -lrdkafka -lconfig -lpthread
CFLAGS += -I../../src/

OBJ = kafka.o
LIB = kafka.so

all: remove_obj link

link: $(OBJ)
	$(LD) $(OBJ) -shared -o $(LIB)

remove_obj:
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL) $(LIB) $(DESTDIR)$(INSTALL_LIBDIR)/$(LIB)

.PHONY: all link remove_obj install
