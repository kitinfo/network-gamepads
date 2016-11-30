.PHONY: clean run
CFLAGS ?= -Wall -g -I/usr/include/libevdev-1.0
server-ng: LDLIBS = -levdev

all: server-ng client

clean:
	$(RM) server-ng client
