SUBDIRS = $(wildcard */.)
INSTALLDIRS = $(patsubst %, install-%, $(SUBDIRS))
CLEANDIRS = $(patsubst %, clean-%, $(SUBDIRS))

all: $(SUBDIRS)

install: $(INSTALLDIRS)

clean: $(CLEANDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

$(INSTALLDIRS):
	$(MAKE) -C $(patsubst install-%, %, $@) install

$(CLEANDIRS):
	$(MAKE) -C $(patsubst clean-%, %, $@) clean

.PHONY: all $(SUBDIRS) $(INSTALLDIRS) $(CLEANDIRS)
