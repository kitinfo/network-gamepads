.PHONY: clean install
PREFIX ?= /usr/local
CFLAGS ?= -Wall -g $(shell pkg-config --cflags libevdev)
LDLIBS ?= -levdev

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c ../common/*.c ../libs/*.c))

DEPS = $(wildcard ../common/*.h ../libs/*.h *.h)

all: input-server input-server.1.gz

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -c -o $@ $<

%.1.gz: %.1
	gzip -c < $< > $@

input-server: $(OBJECTS)

install:
	install -m 0755 -D input-server "$(DESTDIR)$(PREFIX)/bin"
	install -g 0 -o 0 -m 0644 input-server.1.gz $(DESTDIR)$(PREFIX)/share/man/man1/

clean:
	$(RM) input-server
	$(RM) $(OBJECTS)
	$(RM) input-server.1.gz
