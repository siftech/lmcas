PREFIX ?= /usr/local

NAMES := cp1 cp2 cp3 cp4 cp5 cp6 cp8 cp9 cp_sighup_before_neck cp_sighup_after_neck cp_umask cp_fcntl_getfd
BINARIES := $(addprefix bin/,$(NAMES))
OBJECTS := $(patsubst %.c,%.o,$(wildcard *.c))

all: $(BINARIES)
clean:
	test ! -d bin || rm -r bin
	test ! -d tmp || rm -r tmp
install: $(BINARIES)
	install -Dt $(PREFIX)/bin $(BINARIES)
.PHONY: all install clean
.SUFFIXES:

bin/%: tmp/%.o tmp/common.o
	mkdir -p $(dir $@)
	$(CC) -o $@ $^

tmp/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) -c -o $@ $^
