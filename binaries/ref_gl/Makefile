VPATH=../../ref_gl ../../linux ../../game

CFLAGS+=-fPIC $(shell sdl-config --cflags)

ref_gl_SRC:=gl_draw.c gl_image.c gl_light.c gl_mesh.c gl_model.c gl_rmain.c\
			gl_rmisc.c gl_rsurf.c gl_warp.c gl_sdl.c glob.c q_shared.c\
			q_shlinux.c qgl_linux.c

ref_gl_OBJ:=$(ref_gl_SRC:.c=.o)
ALLSRC:=$(ref_gl_SRC)

.PHONY: default combine

default: ref_gl.so

TARGETS:=ref_gl.so ref_gl-combine.so

include ../make.inc

LDFLAGS+=$(shell sdl-config --libs) -lm -lGL -ljpeg -lpng12

ref_gl.so: $(ref_gl_OBJ)
	$(CC) -shared -g -o $@ $^ $(LDFLAGS)

# only works with gcc >4.1
combine: ref_gl-combine.so
ref_gl-combine.so: $(ref_gl_SRC)
	$(CC) $(shell sdl-config --cflags) -shared -O2 -ffloat-store -DNDEBUG -DLINUX -fPIC -fwhole-program --combine -g -o $@ $^ $(LDFLAGS)

-include .depends/*.d
