CC = clang
CFLAGS ?= -O2 -Wall -Wextra
LDLIBS += -framework CoreFoundation -framework DiskArbitration

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

.PHONY: all install clean

all: darof

darof: darof.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

install: darof
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 darof "$(DESTDIR)$(BINDIR)/darof"

clean:
	$(RM) darof
