CFLAGS+=-DNDEBUG -DLINUX -O2 -g -MF .depends/$*.d -MMD
ALLOBJ:=$(ALLSRC:.c=.o)

.PHONY: clean distclean

clean:
	-rm -f $(ALLOBJ) $(TARGETS)

DEPENDS=$(foreach file, $(ALLSRC:.c=.d), .depends/$(file))

distclean: clean
	-rm -f $(DEPENDS)

-include $(DEPENDS)

