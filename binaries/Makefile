.PHONY: default all r1q2ded game client clean distclean

default: all

all: r1q2ded client game

r1q2ded:
	make -C r1q2ded

client:
	make -C client

game:
	make -C game

clean:
	make -C r1q2ded clean
	make -C client clean
	make -C game clean

distclean:
	make -C r1q2ded distclean
	make -C client distclean
	make -C game distclean

