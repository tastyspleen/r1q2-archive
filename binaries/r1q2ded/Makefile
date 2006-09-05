VPATH=../../game ../../linux ../../qcommon ../../server

CFLAGS+=-DDEDICATED_ONLY -DANTICHEAT

r1q2ded_SRC:=cmd.c cmodel.c common.c crc.c cvar.c files.c md4.c net_chan.c \
	     mersennetwister.c redblack.c sv_ccmds.c sv_ents.c sv_game.c \
	     sv_init.c sv_main.c sv_send.c sv_user.c sv_world.c q_shlinux.c \
	     sys_linux.c glob.c net_udp.c q_shared.c pmove.c ioapi.c unzip.c \
	     sv_anticheat.c

r1q2ded_OBJ:=$(r1q2ded_SRC:.c=.o)
ALLSRC:=$(r1q2ded_SRC)

.PHONY: default combine

default: r1q2ded

LDFLAGS=-lm -lz

ifeq ($(shell uname),Linux)
LDFLAGS+=-ldl
endif

ifeq ($(shell uname),FreeBSD)
LDFLAGS+=-lexecinfo
endif


r1q2ded: $(r1q2ded_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# only works with gcc >4.1
combine: r1q2ded-combine
r1q2ded-combine: $(r1q2ded_SRC)
	$(CC) -O2 -ffloat-store -DNDEBUG -DLINUX -DDEDICATED_ONLY -fwhole-program --combine -g -o $@ $^ $(LDFLAGS)

TARGETS:=r1q2ded

include ../make.inc

-include .depends/*.d
