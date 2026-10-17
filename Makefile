CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Wpedantic
LDLIBS = -lX11 -l:libXtst.so.6
PREFIX ?= /usr
all: xfce-alt-scroll
xfce-alt-scroll: src/xfce-alt-scroll.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)
clean:
	rm -f xfce-alt-scroll
install: xfce-alt-scroll
	install -Dm755 xfce-alt-scroll $(DESTDIR)$(PREFIX)/bin/xfce-alt-scroll
.PHONY: all clean install
