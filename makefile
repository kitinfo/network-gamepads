.PHONY: clean

all: input-server input-client osc-xlater

input-server:
	$(MAKE) -C server

input-client:
	$(MAKE) -C client

osc-xlater:
	$(MAKE) -C osc


install: install-server install-client

install-server:
	$(MAKE) -C server install

install-client:
	$(MAKE) -C client install

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean
	$(MAKE) -C osc clean
