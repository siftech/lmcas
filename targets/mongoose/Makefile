PREFIX ?= /usr/local

NAMES := switched-server
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

bin/%: tmp/%.o tmp/mongoose.o
	mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ -Wl,--start-group $^ -Wl,--end-group $(LOADLIBES) $(LDLIBS)

tmp/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -std=c11 -pedantic -Wall -Werror -Wextra -c -o $@ $^
