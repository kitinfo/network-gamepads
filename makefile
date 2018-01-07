.PHONY: clean

all: input-server input-client osc-xlater

input-server:
	$(MAKE) -C server

input-client:
	$(MAKE) -C client

osc-xlater:
	$(MAKE) -C osc

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean
	$(MAKE) -C osc clean
