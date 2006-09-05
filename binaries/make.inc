#turol: for some reason -ffast-math causes strange errors
#and it's not really any faster
CFLAGS+=-DNDEBUG -DLINUX -O2 -ffloat-store -pipe -g -MF .depends/$*.d -MMD
ALLOBJ:=$(ALLSRC:.c=.o)

.PHONY: clean distclean

clean:
	-rm -f $(ALLOBJ) $(TARGETS)

DEPENDS=$(foreach file, $(ALLSRC:.c=.d), .depends/$(file))

distclean: clean
	-rm -f $(DEPENDS)

-include $(DEPENDS)

