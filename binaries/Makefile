.PHONY: default all r1q2ded game client clean distclean

default: all

all: r1q2ded client game

r1q2ded:
	$(MAKE) -C r1q2ded

client:
	$(MAKE) -C client

game:
	$(MAKE) -C game

clean:
	$(MAKE) -C r1q2ded clean
	$(MAKE) -C client clean
	$(MAKE) -C game clean

distclean:
	$(MAKE) -C r1q2ded distclean
	$(MAKE) -C client distclean
	$(MAKE) -C game distclean
