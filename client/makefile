.PHONY: clean install
PREFIX ?= $(DESTDIR)/usr/local/bin
CFLAGS ?= -Wall -g

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c ../common/*.c ../libs/*.c))

DEPS = $(wildcard ../common/*.h ../libs/*.h *.h)

all: input-client input-client.1.gz

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -c -o $@ $<

%.1.gz: %.1
	gzip -c < $< > $@

input-client: $(OBJECTS)

install:
	install -m 0755 -D input-client "$(DESTDIR)$(PREFIX)"
	install -g 0 -o 0 -m 0644 input-client.1.gz $(DESTDIR)$(PREFIX)/share/man/man1/

clean:
	$(RM) input-client
	$(RM) $(OBJECTS)
	$(RM) input-client.1.gz
