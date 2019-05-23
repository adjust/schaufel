SCHAUFEL ?= schaufel

ifdef CONTRIB
	include ../../Makefile.global
	CPPFLAGS += -I../../src/include
else
	include $(shell $(SCHAUFEL) -L)/Makefile.global
	CPPFLAGS += -I$(shell $(SCHAUFEL) -I)
endif

CFLAGS += -fPIC
LDFLAGS += -shared

all: clean $(LIB)

$(LIB): $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) $< -o $@

clean:
	rm -f $(OBJ)
	rm -f $(LIB)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: all
	$(INSTALL) $(LIB) $(contribdir)/$(LIB)

.PHONY: all link clean install
