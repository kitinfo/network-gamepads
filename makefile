.PHONY: clean run install
PREFIX ?= $(DESTDIR)/usr/bin
CFLAGS ?= -Wall -g

all: server-ng client

install-server: server-ng
	mv server-ng input-server
	install -m 0755 input-server "$(PREFIX)"

install-client: client
	mv client input-client
	install -m 0755 input-client "$(PREFIX)"

install: install-client install-server

clean:
	$(RM) server-ng client input-server input-client
