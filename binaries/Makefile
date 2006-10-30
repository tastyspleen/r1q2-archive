.PHONY: default all r1q2ded game client ref_gl clean distclean combine

default: all

all: r1q2ded client game ref_gl

r1q2ded:
	$(MAKE) -C r1q2ded

client:
	$(MAKE) -C client

game:
	$(MAKE) -C game

ref_gl:
	$(MAKE) -C ref_gl

clean:
	$(MAKE) -C r1q2ded clean
	$(MAKE) -C client clean
	$(MAKE) -C game clean
	$(MAKE) -C ref_gl clean

distclean:
	$(MAKE) -C r1q2ded distclean
	$(MAKE) -C client distclean
	$(MAKE) -C game distclean
	$(MAKE) -C ref_gl distclean

combine:
	$(MAKE) -C r1q2ded combine
	$(MAKE) -C client combine
	$(MAKE) -C game combine
	$(MAKE) -C ref_gl combine
