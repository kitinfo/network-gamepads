.PHONY: clean install
PREFIX ?= $(DESTDIR)/usr/local/bin
CFLAGS ?= -Wall -g

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c ../common/*.c ../libs/*.c))

DEPS = $(wildcard ../common/*.h ../libs/*.h *.h)

all: input-client

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -c -o $@ $<

input-client: $(OBJECTS)

install:
	install -m 0755 -D input-client "$(DESTDIR)$(PREFIX)/bin"

clean:
	$(RM) input-client
	$(RM) $(OBJECTS)
