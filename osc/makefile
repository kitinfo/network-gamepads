.PHONY: clean install
PREFIX ?= /usr/local
CFLAGS ?= -Wall -g
LDLIBS ?= -lm

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c ../common/*.c ../libs/*.c))

DEPS = $(wildcard ../common/*.h ../libs/*.h *.h)

all: osc-xlater

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -c -o $@ $<

osc-xlater: $(OBJECTS)

install:
	install -m 0755 -D osc-xlater "$(DESTDIR)$(PREFIX)/bin"

clean:
	$(RM) osc-xlater
	$(RM) $(OBJECTS)
