.PHONY: clean install
PREFIX ?= $(DESTDIR)/usr/local/bin
CFLAGS ?= -Wall -g

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c ../common/*.c ../libs/*.c))

DEPS = $(wildcard ../common/*.h ../libs/*.h *.h)

all: input-server

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -c -o $@ $<

input-server: $(OBJECTS)

install:
	install -m 0755 -D input-server "$(DESTDIR)$(PREFIX)/bin"

clean:
	$(RM) input-server
	$(RM) $(OBJECTS)