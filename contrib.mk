SCHAUFEL ?= schaufel

ifdef CONTRIB
	include ../../Makefile.global
	CFLAGS += -I../../src/include
else
	include $(shell $(SCHAUFEL) -L)/Makefile.global
	CFLAGS += -I$(shell $(SCHAUFEL) -I)
endif

CFLAGS += -fPIC
CFLAGS += $(LIBS)

all: clean link

link: $(OBJ)
	$(LD) $(OBJ) -shared -o $(LIB)

clean:
	rm -f $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL) $(LIB) $(contribdir)/$(LIB)

.PHONY: all link clean install
