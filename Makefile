#
# Quake2 Makefile for Linux 2.0
#
# Nov '97 by Zoid <zoid@idsoftware.com>
#
# ELF only
#

# start of configurable options

# Here are your build options, no more will be added!
# (Note: not all options are available for all platforms).
# quake2 (uses OSS for sound, cdrom ioctls for cd audio) is automatically built.
# gamei386.so is automatically built.
BUILD_SVGA=YES		# SVGAlib driver. Seems to work fine.
BUILD_X11=YES		# X11 software driver. Works somewhat ok.
BUILD_GLX=YES		# X11 GLX driver. Works somewhat ok.

# Check OS type.
OSTYPE := $(shell uname -s)

ifneq ($(OSTYPE),Linux)
ifneq ($(OSTYPE),FreeBSD)
$(error OS $(OSTYPE) is currently not supported)
endif
endif

# this nice line comes from the linux kernel makefile
ARCH := $(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

ifneq ($(ARCH),i386)
ifneq ($(ARCH),axp)
ifneq ($(ARCH),ppc)
$(error arch $(ARCH) is currently not supported)
endif
endif
endif

CC=gcc

ifeq ($(ARCH),axp)
RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(ARCH),ppc)
RELEASE_CFLAGS=$(BASE_CFLAGS) -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations
endif

ifeq ($(ARCH),i386)
RELEASE_CFLAGS=$(BASE_CFLAGS) -O6 -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations \
#	-falign-loops=2 -falign-jumps=2 -falign-functions=2
#RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -falign-loops=2 \
#	-falign-jumps=2 -falign-functions=2 -g -Wall
#RELEASE_CFLAGS=$(BASE_CFLAGS) -O2 -ffast-math -funroll-loops -malign-loops=2 \
#	-malign-jumps=2 -malign-functions=2 -g -Wall
# compiler bugs with gcc 2.96 and 3.0.1 can cause bad builds with heavy opts.
#RELEASE_CFLAGS=$(BASE_CFLAGS) -O6 -march=i686 -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations -falign-loops=2 \
	-falign-jumps=2 -falign-functions=2
endif

# (hopefully) end of configurable options
VERSION=3.21

MOUNT_DIR=.

BUILD_DEBUG_DIR=debug$(ARCH)
BUILD_RELEASE_DIR=release$(ARCH)

CLIENT_DIR=$(MOUNT_DIR)/client

SERVER_DIR=$(MOUNT_DIR)/server
NULL_DIR=$(MOUNT_DIR)/null

REF_SOFT_DIR=$(MOUNT_DIR)/ref_soft
REF_GL_DIR=$(MOUNT_DIR)/ref_gl
COMMON_DIR=$(MOUNT_DIR)/qcommon
LINUX_DIR=$(MOUNT_DIR)/linux
GAME_DIR=$(MOUNT_DIR)/game

BASE_CFLAGS=-Dstricmp=strcasecmp

DEBUG_CFLAGS=$(BASE_CFLAGS) -g -Wall

ifeq ($(OSTYPE),FreeBSD)
LDFLAGS=-lm -lz
else
LDFLAGS=-lm -ldl -lz
endif

SVGALDFLAGS=-lvga

XCFLAGS=
XLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm

FXGLCFLAGS=
FXGLLDFLAGS=-L/usr/local/glide/lib -L/usr/X11/lib -L/usr/local/lib \
	-L/usr/X11R6/lib -lX11 -lXext -lGL -lvga

GLXCFLAGS=-DWITH_EVDEV
GLXLDFLAGS=-L/usr/X11R6/lib -ljpeg -lX11 -lXext -lXxf86dga -lXxf86vm

SHLIBEXT=so

SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<

DO_DED_CC=$(CC) $(CFLAGS) -DDEDICATED_ONLY -DNDEBUG -DLINUX -o $@ -c $<

DO_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
DO_GL_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) $(GLCFLAGS) -o $@ -c $<
DO_AS=$(CC) $(CFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<
DO_SHLIB_AS=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<

#############################################################################
# SETUP AND BUILD
#############################################################################

.PHONY : targets build_debug build_release clean clean-debug clean-release clean2

CL_TARGETS=$(BUILDDIR)/r1q2 $(BUILDDIR)/game$(ARCH).$(SHLIBEXT)
SV_TARGETS=$(BUILDDIR)/r1q2ded

ifeq ($(ARCH),axp)

 ifeq ($(strip $(BUILD_SVGA)),YES)
  $(warning Warning: SVGAlib support not supported for $(ARCH))
 endif

 ifeq ($(strip $(BUILD_X11)),YES)
  $(warning Warning: X11 support not supported for $(ARCH))
 endif

 ifeq ($(strip $(BUILD_GLX)),YES)
  $(warning Warning: support not supported for $(ARCH))
 endif

 ifeq ($(strip $(BUILD_FXGL)),YES)
  $(warning Warning: FXGL support not supported for $(ARCH))
 endif

endif # ARCH axp

ifeq ($(ARCH),ppc)
 ifeq ($(strip $(BUILD_SVGA)),YES)
  $(warning Warning: SVGAlib support not supported for $(ARCH))
 endif

 ifeq ($(strip $(BUILD_X11)),YES)
  $(warning Warning: X11 support not supported for $(ARCH))
 endif

 ifeq ($(strip $(BUILD_GLX)),YES)
  $(warning Warning: GLX support not supported for $(ARCH))
 endif

 ifeq ($(strip $(BUILD_FXGL)),YES)
  $(warning Warning: FXGL support not supported for $(ARCH))
 endif

endif # ARCH ppc
	
ifeq ($(ARCH),i386)

 ifeq ($(strip $(BUILD_SVGA)),YES)
  CL_TARGETS += $(BUILDDIR)/ref_soft.$(SHLIBEXT)
 endif

 ifeq ($(strip $(BUILD_X11)),YES)
  CL_TARGETS += $(BUILDDIR)/ref_softx.$(SHLIBEXT)
 endif

 ifeq ($(strip $(BUILD_GLX)),YES)
  CL_TARGETS += $(BUILDDIR)/ref_glx.$(SHLIBEXT)
 endif

 ifeq ($(strip $(BUILD_FXGL)),YES)
  CL_TARGETS += $(BUILDDIR)/ref_gl.$(SHLIBEXT)
 endif

endif # ARCH i386

all: 
	@echo == R1Q2 Makefile ==
	@echo possible r1q2 makefile targets: 
	@echo " \"build_client_release\"  to build the client for release"
	@echo " \"build_client_debug\"    to build the client with compiler debug options"
	@echo " \"build_q2ded_release\"   to build the r1q2 dedicated server"
	@echo 
	@echo " \"clean\"         to clean off all .o files"
	@echo " \"clean-debug\"   to only kill the debug .o"
	@echo " \"clean-release\" to only kill the release .o"

build_client_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR) \
		$(BUILD_DEBUG_DIR)/client \
		$(BUILD_DEBUG_DIR)/ref_soft \
		$(BUILD_DEBUG_DIR)/ref_gl \
		$(BUILD_DEBUG_DIR)/game 
	# now make	
	$(MAKE) cl_targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

build_client_release:
	@-mkdir -p $(BUILD_RELEASE_DIR) \
		$(BUILD_RELEASE_DIR)/client \
		$(BUILD_RELEASE_DIR)/ref_soft \
		$(BUILD_RELEASE_DIR)/ref_gl \
		$(BUILD_RELEASE_DIR)/game 
	# now make	
	$(MAKE) cl_targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"

build_q2ded_release:
	@-mkdir -p $(BUILD_RELEASE_DIR) \
		$(BUILD_RELEASE_DIR)/svded
	# make the q2ded
	$(MAKE) sv_targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"





sv_targets: $(SV_TARGETS)
#############################################################################
# SERVER dedicated
#############################################################################

Q2DED_OBJS = \
	\
	$(BUILDDIR)/svded/cmd.o \
	$(BUILDDIR)/svded/cmodel.o \
	$(BUILDDIR)/svded/common.o \
	$(BUILDDIR)/svded/crc.o \
	$(BUILDDIR)/svded/cvar.o \
	$(BUILDDIR)/svded/files.o \
	$(BUILDDIR)/svded/md4.o \
	$(BUILDDIR)/svded/net_chan.o \
	$(BUILDDIR)/svded/mersennetwister.o \
	$(BUILDDIR)/svded/redblack.o \
	\
	$(BUILDDIR)/svded/sv_ccmds.o \
	$(BUILDDIR)/svded/sv_ents.o \
	$(BUILDDIR)/svded/sv_game.o \
	$(BUILDDIR)/svded/sv_init.o \
	$(BUILDDIR)/svded/sv_main.o \
	$(BUILDDIR)/svded/sv_send.o \
	$(BUILDDIR)/svded/sv_user.o \
	$(BUILDDIR)/svded/sv_world.o \
	\
	$(BUILDDIR)/svded/q_shlinux.o \
	$(BUILDDIR)/svded/sys_linux.o \
	$(BUILDDIR)/svded/glob.o \
	$(BUILDDIR)/svded/net_udp.o \
	\
	$(BUILDDIR)/svded/q_shared.o \
	$(BUILDDIR)/svded/pmove.o \
	\
	$(BUILDDIR)/svded/cl_null.o \
	$(BUILDDIR)/svded/cd_null.o

	
$(BUILDDIR)/r1q2ded : $(Q2DED_OBJS)
	$(CC) $(CFLAGS) -o $@ $(Q2DED_OBJS) $(LDFLAGS)
	
$(BUILDDIR)/svded/cmd.o :        $(COMMON_DIR)/cmd.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/cmodel.o :     $(COMMON_DIR)/cmodel.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/common.o :     $(COMMON_DIR)/common.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/crc.o :        $(COMMON_DIR)/crc.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/cvar.o :       $(COMMON_DIR)/cvar.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/files.o :      $(COMMON_DIR)/files.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/md4.o :        $(COMMON_DIR)/md4.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/net_chan.o :   $(COMMON_DIR)/net_chan.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/mersennetwister.o :  $(COMMON_DIR)/mersennetwister.c
	$(DO_DED_CC)

$(BUILDDIR)/svded/redblack.o :  $(COMMON_DIR)/redblack.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/q_shared.o :   $(GAME_DIR)/q_shared.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/pmove.o :      $(COMMON_DIR)/pmove.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_ccmds.o :   $(SERVER_DIR)/sv_ccmds.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_ents.o :    $(SERVER_DIR)/sv_ents.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_game.o :    $(SERVER_DIR)/sv_game.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_init.o :    $(SERVER_DIR)/sv_init.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_main.o :    $(SERVER_DIR)/sv_main.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_send.o :    $(SERVER_DIR)/sv_send.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_user.o :    $(SERVER_DIR)/sv_user.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sv_world.o :   $(SERVER_DIR)/sv_world.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/q_shlinux.o :  $(LINUX_DIR)/q_shlinux.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/sys_linux.o :  $(LINUX_DIR)/sys_linux.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/glob.o :       $(LINUX_DIR)/glob.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/net_udp.o :    $(LINUX_DIR)/net_udp.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/cl_null.o :    $(NULL_DIR)/cl_null.c
	$(DO_DED_CC)
	
$(BUILDDIR)/svded/cd_null.o :    $(NULL_DIR)/cd_null.c
	$(DO_DED_CC)
	



cl_targets: $(CL_TARGETS)
#############################################################################
# CLIENT/SERVER hybrid
#############################################################################

QUAKE2_OBJS = \
	$(BUILDDIR)/client/cl_cin.o \
	$(BUILDDIR)/client/cl_ents.o \
	$(BUILDDIR)/client/cl_fx.o \
	$(BUILDDIR)/client/cl_input.o \
	$(BUILDDIR)/client/cl_inv.o \
	$(BUILDDIR)/client/cl_main.o \
	$(BUILDDIR)/client/cl_parse.o \
	$(BUILDDIR)/client/cl_pred.o \
	$(BUILDDIR)/client/cl_tent.o \
	$(BUILDDIR)/client/cl_scrn.o \
	$(BUILDDIR)/client/cl_view.o \
	$(BUILDDIR)/client/cl_newfx.o \
	$(BUILDDIR)/client/console.o \
	$(BUILDDIR)/client/keys.o \
	$(BUILDDIR)/client/menu.o \
	$(BUILDDIR)/client/snd_dma.o \
	$(BUILDDIR)/client/snd_mem.o \
	$(BUILDDIR)/client/snd_mix.o \
	$(BUILDDIR)/client/qmenu.o \
	$(BUILDDIR)/client/m_flash.o \
	\
	$(BUILDDIR)/client/cmd.o \
	$(BUILDDIR)/client/cmodel.o \
	$(BUILDDIR)/client/common.o \
	$(BUILDDIR)/client/crc.o \
	$(BUILDDIR)/client/cvar.o \
	$(BUILDDIR)/client/files.o \
	$(BUILDDIR)/client/md4.o \
	$(BUILDDIR)/client/net_chan.o \
	\
	$(BUILDDIR)/client/sv_ccmds.o \
	$(BUILDDIR)/client/sv_ents.o \
	$(BUILDDIR)/client/sv_game.o \
	$(BUILDDIR)/client/sv_init.o \
	$(BUILDDIR)/client/sv_main.o \
	$(BUILDDIR)/client/sv_send.o \
	$(BUILDDIR)/client/sv_user.o \
	$(BUILDDIR)/client/sv_world.o \
	\
	$(BUILDDIR)/client/q_shlinux.o \
	$(BUILDDIR)/client/vid_menu.o \
	$(BUILDDIR)/client/vid_so.o \
	$(BUILDDIR)/client/sys_linux.o \
	$(BUILDDIR)/client/glob.o \
	$(BUILDDIR)/client/net_udp.o \
	\
	$(BUILDDIR)/client/q_shared.o \
	$(BUILDDIR)/client/pmove.o \
	$(BUILDDIR)/client/snd_openal.o \
	$(BUILDDIR)/client/mersennetwister.o \
	$(BUILDDIR)/client/le_util.o \
	$(BUILDDIR)/client/le_physics.o

QUAKE2_LNX_OBJS = \
	$(BUILDDIR)/client/cd_linux.o \
	$(BUILDDIR)/client/snd_linux.o

ifeq ($(ARCH),axp)
QUAKE2_AS_OBJS =  #blank
else
QUAKE2_AS_OBJS = \
	$(BUILDDIR)/client/snd_mixa.o
endif

$(BUILDDIR)/r1q2 : $(QUAKE2_OBJS) $(QUAKE2_LNX_OBJS) $(QUAKE2_AS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(QUAKE2_OBJS) $(QUAKE2_LNX_OBJS) $(QUAKE2_AS_OBJS) $(LDFLAGS)


$(BUILDDIR)/client/cl_cin.o :     $(CLIENT_DIR)/cl_cin.c
	$(DO_CC)

$(BUILDDIR)/client/cl_ents.o :    $(CLIENT_DIR)/cl_ents.c
	$(DO_CC)

$(BUILDDIR)/client/cl_fx.o :      $(CLIENT_DIR)/cl_fx.c
	$(DO_CC)

$(BUILDDIR)/client/cl_input.o :   $(CLIENT_DIR)/cl_input.c
	$(DO_CC)

$(BUILDDIR)/client/cl_inv.o :     $(CLIENT_DIR)/cl_inv.c
	$(DO_CC)

$(BUILDDIR)/client/cl_main.o :    $(CLIENT_DIR)/cl_main.c
	$(DO_CC)

$(BUILDDIR)/client/cl_parse.o :   $(CLIENT_DIR)/cl_parse.c
	$(DO_CC)

$(BUILDDIR)/client/cl_pred.o :    $(CLIENT_DIR)/cl_pred.c
	$(DO_CC)

$(BUILDDIR)/client/cl_tent.o :    $(CLIENT_DIR)/cl_tent.c
	$(DO_CC)

$(BUILDDIR)/client/cl_scrn.o :    $(CLIENT_DIR)/cl_scrn.c
	$(DO_CC)

$(BUILDDIR)/client/cl_view.o :    $(CLIENT_DIR)/cl_view.c
	$(DO_CC)

$(BUILDDIR)/client/cl_newfx.o :	  $(CLIENT_DIR)/cl_newfx.c
	$(DO_CC)

$(BUILDDIR)/client/console.o :    $(CLIENT_DIR)/console.c
	$(DO_CC)

#$(BUILDDIR)/client/cl_loc.o :    $(CLIENT_DIR)/cl_loc.c
#	$(DO_CC)

$(BUILDDIR)/client/keys.o :       $(CLIENT_DIR)/keys.c
	$(DO_CC)

$(BUILDDIR)/client/menu.o :       $(CLIENT_DIR)/menu.c
	$(DO_CC)

$(BUILDDIR)/client/snd_dma.o :    $(CLIENT_DIR)/snd_dma.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mem.o :    $(CLIENT_DIR)/snd_mem.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mix.o :    $(CLIENT_DIR)/snd_mix.c
	$(DO_CC)

$(BUILDDIR)/client/qmenu.o :      $(CLIENT_DIR)/qmenu.c
	$(DO_CC)
	
$(BUILDDIR)/client/le_util.o :      $(CLIENT_DIR)/le_util.c
	$(DO_CC)
	
$(BUILDDIR)/client/le_physics.o :      $(CLIENT_DIR)/le_physics.c
	$(DO_CC)

$(BUILDDIR)/client/m_flash.o :    $(GAME_DIR)/m_flash.c
	$(DO_CC)

$(BUILDDIR)/client/cmd.o :        $(COMMON_DIR)/cmd.c
	$(DO_CC)

$(BUILDDIR)/client/cmodel.o :     $(COMMON_DIR)/cmodel.c
	$(DO_CC)

$(BUILDDIR)/client/common.o :     $(COMMON_DIR)/common.c
	$(DO_CC)

$(BUILDDIR)/client/crc.o :        $(COMMON_DIR)/crc.c
	$(DO_CC)

$(BUILDDIR)/client/cvar.o :       $(COMMON_DIR)/cvar.c
	$(DO_CC)

$(BUILDDIR)/client/files.o :      $(COMMON_DIR)/files.c
	$(DO_CC)

$(BUILDDIR)/client/md4.o :        $(COMMON_DIR)/md4.c
	$(DO_CC)

$(BUILDDIR)/client/net_chan.o :   $(COMMON_DIR)/net_chan.c
	$(DO_CC)

$(BUILDDIR)/client/q_shared.o :   $(GAME_DIR)/q_shared.c
	$(DO_CC)

$(BUILDDIR)/client/pmove.o :      $(COMMON_DIR)/pmove.c
	$(DO_CC)

$(BUILDDIR)/client/sv_ccmds.o :   $(SERVER_DIR)/sv_ccmds.c
	$(DO_CC)

$(BUILDDIR)/client/sv_ents.o :    $(SERVER_DIR)/sv_ents.c
	$(DO_CC)

$(BUILDDIR)/client/sv_game.o :    $(SERVER_DIR)/sv_game.c
	$(DO_CC)

$(BUILDDIR)/client/sv_init.o :    $(SERVER_DIR)/sv_init.c
	$(DO_CC)

$(BUILDDIR)/client/sv_main.o :    $(SERVER_DIR)/sv_main.c
	$(DO_CC)

$(BUILDDIR)/client/sv_send.o :    $(SERVER_DIR)/sv_send.c
	$(DO_CC)

$(BUILDDIR)/client/sv_user.o :    $(SERVER_DIR)/sv_user.c
	$(DO_CC)

$(BUILDDIR)/client/sv_world.o :   $(SERVER_DIR)/sv_world.c
	$(DO_CC)

$(BUILDDIR)/client/q_shlinux.o :  $(LINUX_DIR)/q_shlinux.c
	$(DO_CC)

$(BUILDDIR)/client/vid_menu.o :   $(LINUX_DIR)/vid_menu.c
	$(DO_CC)

$(BUILDDIR)/client/vid_so.o :     $(LINUX_DIR)/vid_so.c
	$(DO_CC)

$(BUILDDIR)/client/snd_mixa.o :   $(LINUX_DIR)/snd_mixa.s
	$(DO_AS)

$(BUILDDIR)/client/sys_linux.o :  $(LINUX_DIR)/sys_linux.c
	$(DO_CC)

$(BUILDDIR)/client/glob.o :       $(LINUX_DIR)/glob.c
	$(DO_CC)

$(BUILDDIR)/client/net_udp.o :    $(LINUX_DIR)/net_udp.c
	$(DO_CC)

$(BUILDDIR)/client/cd_linux.o :   $(LINUX_DIR)/cd_linux.c
	$(DO_CC)

$(BUILDDIR)/client/snd_linux.o :  $(LINUX_DIR)/snd_linux.c
	$(DO_CC)

$(BUILDDIR)/client/mersennetwister.o : $(COMMON_DIR)/mersennetwister.c
	$(DO_CC)

$(BUILDDIR)/client/snd_openal.o : $(CLIENT_DIR)/snd_openal.c
	$(DO_CC)

#############################################################################
# GAME
#############################################################################

GAME_OBJS = \
	$(BUILDDIR)/game/g_ai.o \
	$(BUILDDIR)/game/p_client.o \
	$(BUILDDIR)/game/g_chase.o \
	$(BUILDDIR)/game/g_cmds.o \
	$(BUILDDIR)/game/g_svcmds.o \
	$(BUILDDIR)/game/g_combat.o \
	$(BUILDDIR)/game/g_func.o \
	$(BUILDDIR)/game/g_items.o \
	$(BUILDDIR)/game/g_main.o \
	$(BUILDDIR)/game/g_misc.o \
	$(BUILDDIR)/game/g_monster.o \
	$(BUILDDIR)/game/g_phys.o \
	$(BUILDDIR)/game/g_save.o \
	$(BUILDDIR)/game/g_spawn.o \
	$(BUILDDIR)/game/g_target.o \
	$(BUILDDIR)/game/g_trigger.o \
	$(BUILDDIR)/game/g_turret.o \
	$(BUILDDIR)/game/g_utils.o \
	$(BUILDDIR)/game/g_weapon.o \
	$(BUILDDIR)/game/m_actor.o \
	$(BUILDDIR)/game/m_berserk.o \
	$(BUILDDIR)/game/m_boss2.o \
	$(BUILDDIR)/game/m_boss3.o \
	$(BUILDDIR)/game/m_boss31.o \
	$(BUILDDIR)/game/m_boss32.o \
	$(BUILDDIR)/game/m_brain.o \
	$(BUILDDIR)/game/m_chick.o \
	$(BUILDDIR)/game/m_flipper.o \
	$(BUILDDIR)/game/m_float.o \
	$(BUILDDIR)/game/m_flyer.o \
	$(BUILDDIR)/game/m_gladiator.o \
	$(BUILDDIR)/game/m_gunner.o \
	$(BUILDDIR)/game/m_hover.o \
	$(BUILDDIR)/game/m_infantry.o \
	$(BUILDDIR)/game/m_insane.o \
	$(BUILDDIR)/game/m_medic.o \
	$(BUILDDIR)/game/m_move.o \
	$(BUILDDIR)/game/m_mutant.o \
	$(BUILDDIR)/game/m_parasite.o \
	$(BUILDDIR)/game/m_soldier.o \
	$(BUILDDIR)/game/m_supertank.o \
	$(BUILDDIR)/game/m_tank.o \
	$(BUILDDIR)/game/p_hud.o \
	$(BUILDDIR)/game/p_trail.o \
	$(BUILDDIR)/game/p_view.o \
	$(BUILDDIR)/game/p_weapon.o \
	$(BUILDDIR)/game/q_shared.o \
	$(BUILDDIR)/game/m_flash.o

$(BUILDDIR)/game$(ARCH).$(SHLIBEXT) : $(GAME_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(GAME_OBJS)

$(BUILDDIR)/game/g_ai.o :        $(GAME_DIR)/g_ai.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_chase.o :     $(GAME_DIR)/g_chase.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/p_client.o :    $(GAME_DIR)/p_client.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_cmds.o :      $(GAME_DIR)/g_cmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_svcmds.o :    $(GAME_DIR)/g_svcmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_combat.o :    $(GAME_DIR)/g_combat.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_func.o :      $(GAME_DIR)/g_func.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_items.o :     $(GAME_DIR)/g_items.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_main.o :      $(GAME_DIR)/g_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_misc.o :      $(GAME_DIR)/g_misc.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_monster.o :   $(GAME_DIR)/g_monster.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_phys.o :      $(GAME_DIR)/g_phys.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_save.o :      $(GAME_DIR)/g_save.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_spawn.o :     $(GAME_DIR)/g_spawn.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_target.o :    $(GAME_DIR)/g_target.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_trigger.o :   $(GAME_DIR)/g_trigger.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_turret.o :    $(GAME_DIR)/g_turret.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_utils.o :     $(GAME_DIR)/g_utils.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/g_weapon.o :    $(GAME_DIR)/g_weapon.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_actor.o :     $(GAME_DIR)/m_actor.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_berserk.o :   $(GAME_DIR)/m_berserk.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_boss2.o :     $(GAME_DIR)/m_boss2.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_boss3.o :     $(GAME_DIR)/m_boss3.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_boss31.o :     $(GAME_DIR)/m_boss31.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_boss32.o :     $(GAME_DIR)/m_boss32.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_brain.o :     $(GAME_DIR)/m_brain.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_chick.o :     $(GAME_DIR)/m_chick.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_flipper.o :   $(GAME_DIR)/m_flipper.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_float.o :     $(GAME_DIR)/m_float.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_flyer.o :     $(GAME_DIR)/m_flyer.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_gladiator.o : $(GAME_DIR)/m_gladiator.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_gunner.o :    $(GAME_DIR)/m_gunner.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_hover.o :     $(GAME_DIR)/m_hover.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_infantry.o :  $(GAME_DIR)/m_infantry.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_insane.o :    $(GAME_DIR)/m_insane.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_medic.o :     $(GAME_DIR)/m_medic.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_move.o :      $(GAME_DIR)/m_move.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_mutant.o :    $(GAME_DIR)/m_mutant.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_parasite.o :  $(GAME_DIR)/m_parasite.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_soldier.o :   $(GAME_DIR)/m_soldier.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_supertank.o : $(GAME_DIR)/m_supertank.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_tank.o :      $(GAME_DIR)/m_tank.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/p_hud.o :       $(GAME_DIR)/p_hud.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/p_trail.o :     $(GAME_DIR)/p_trail.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/p_view.o :      $(GAME_DIR)/p_view.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/p_weapon.o :    $(GAME_DIR)/p_weapon.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/q_shared.o :    $(GAME_DIR)/q_shared.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/game/m_flash.o :     $(GAME_DIR)/m_flash.c
	$(DO_SHLIB_CC)


#############################################################################
# REF_SOFT
#############################################################################

REF_SOFT_OBJS = \
	$(BUILDDIR)/ref_soft/r_aclip.o \
	$(BUILDDIR)/ref_soft/r_alias.o \
	$(BUILDDIR)/ref_soft/r_bsp.o \
	$(BUILDDIR)/ref_soft/r_draw.o \
	$(BUILDDIR)/ref_soft/r_edge.o \
	$(BUILDDIR)/ref_soft/r_image.o \
	$(BUILDDIR)/ref_soft/r_light.o \
	$(BUILDDIR)/ref_soft/r_main.o \
	$(BUILDDIR)/ref_soft/r_misc.o \
	$(BUILDDIR)/ref_soft/r_model.o \
	$(BUILDDIR)/ref_soft/r_part.o \
	$(BUILDDIR)/ref_soft/r_poly.o \
	$(BUILDDIR)/ref_soft/r_polyse.o \
	$(BUILDDIR)/ref_soft/r_rast.o \
	$(BUILDDIR)/ref_soft/r_scan.o \
	$(BUILDDIR)/ref_soft/r_sprite.o \
	$(BUILDDIR)/ref_soft/r_surf.o \
	\
	$(BUILDDIR)/ref_soft/r_aclipa.o \
	$(BUILDDIR)/ref_soft/r_draw16.o \
	$(BUILDDIR)/ref_soft/r_drawa.o \
	$(BUILDDIR)/ref_soft/r_edgea.o \
	$(BUILDDIR)/ref_soft/r_scana.o \
	$(BUILDDIR)/ref_soft/r_spr8.o \
	$(BUILDDIR)/ref_soft/r_surf8.o \
	$(BUILDDIR)/ref_soft/math.o \
	$(BUILDDIR)/ref_soft/d_polysa.o \
	$(BUILDDIR)/ref_soft/r_varsa.o \
	$(BUILDDIR)/ref_soft/sys_dosa.o \
	\
	$(BUILDDIR)/ref_soft/q_shared.o \
	$(BUILDDIR)/ref_soft/q_shlinux.o \
	$(BUILDDIR)/ref_soft/glob.o

REF_SOFT_SVGA_OBJS = \
	$(BUILDDIR)/ref_soft/rw_svgalib.o \
	$(BUILDDIR)/ref_soft/d_copy.o \
	$(BUILDDIR)/ref_soft/rw_in_svgalib.o

REF_SOFT_X11_OBJS = \
	$(BUILDDIR)/ref_soft/rw_x11.o

$(BUILDDIR)/ref_soft.$(SHLIBEXT) : $(REF_SOFT_OBJS) $(REF_SOFT_SVGA_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_SOFT_OBJS) \
		$(REF_SOFT_SVGA_OBJS) $(SVGALDFLAGS)

$(BUILDDIR)/ref_softx.$(SHLIBEXT) : $(REF_SOFT_OBJS) $(REF_SOFT_X11_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_SOFT_OBJS) \
		$(REF_SOFT_X11_OBJS) $(XLDFLAGS)

$(BUILDDIR)/ref_soft/r_aclip.o :      $(REF_SOFT_DIR)/r_aclip.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_alias.o :      $(REF_SOFT_DIR)/r_alias.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_bsp.o :        $(REF_SOFT_DIR)/r_bsp.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_draw.o :       $(REF_SOFT_DIR)/r_draw.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_edge.o :       $(REF_SOFT_DIR)/r_edge.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_image.o :      $(REF_SOFT_DIR)/r_image.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_light.o :      $(REF_SOFT_DIR)/r_light.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_main.o :       $(REF_SOFT_DIR)/r_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_misc.o :       $(REF_SOFT_DIR)/r_misc.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_model.o :      $(REF_SOFT_DIR)/r_model.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_part.o :       $(REF_SOFT_DIR)/r_part.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_poly.o :       $(REF_SOFT_DIR)/r_poly.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_polyse.o :     $(REF_SOFT_DIR)/r_polyse.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_rast.o :       $(REF_SOFT_DIR)/r_rast.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_scan.o :       $(REF_SOFT_DIR)/r_scan.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_sprite.o :     $(REF_SOFT_DIR)/r_sprite.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_surf.o :       $(REF_SOFT_DIR)/r_surf.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/r_aclipa.o :     $(LINUX_DIR)/r_aclipa.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_draw16.o :     $(LINUX_DIR)/r_draw16.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_drawa.o :      $(LINUX_DIR)/r_drawa.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_edgea.o :      $(LINUX_DIR)/r_edgea.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_scana.o :      $(LINUX_DIR)/r_scana.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_spr8.o :       $(LINUX_DIR)/r_spr8.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_surf8.o :      $(LINUX_DIR)/r_surf8.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/math.o :         $(LINUX_DIR)/math.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/d_polysa.o :     $(LINUX_DIR)/d_polysa.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/r_varsa.o :      $(LINUX_DIR)/r_varsa.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/sys_dosa.o :     $(LINUX_DIR)/sys_dosa.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/q_shared.o :     $(GAME_DIR)/q_shared.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/q_shlinux.o :    $(LINUX_DIR)/q_shlinux.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/glob.o :         $(LINUX_DIR)/glob.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/d_copy.o :       $(LINUX_DIR)/d_copy.s
	$(DO_SHLIB_AS)

$(BUILDDIR)/ref_soft/rw_svgalib.o :   $(LINUX_DIR)/rw_svgalib.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/rw_in_svgalib.o : $(LINUX_DIR)/rw_in_svgalib.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ref_soft/rw_x11.o :       $(LINUX_DIR)/rw_x11.c
	$(DO_SHLIB_CC)


#############################################################################
# REF_GL
#############################################################################

REF_GL_OBJS = \
	$(BUILDDIR)/ref_gl/gl_draw.o \
	$(BUILDDIR)/ref_gl/gl_image.o \
	$(BUILDDIR)/ref_gl/gl_light.o \
	$(BUILDDIR)/ref_gl/gl_mesh.o \
	$(BUILDDIR)/ref_gl/gl_model.o \
	$(BUILDDIR)/ref_gl/gl_rmain.o \
	$(BUILDDIR)/ref_gl/gl_rmisc.o \
	$(BUILDDIR)/ref_gl/gl_rsurf.o \
	$(BUILDDIR)/ref_gl/gl_warp.o \
	\
	$(BUILDDIR)/ref_gl/qgl_linux.o \
	$(BUILDDIR)/ref_gl/q_shared.o \
	$(BUILDDIR)/ref_gl/q_shlinux.o \
	$(BUILDDIR)/ref_gl/glob.o

REF_GLX_OBJS = \
	$(BUILDDIR)/ref_gl/gl_glx.o

REF_FXGL_OBJS = \
	$(BUILDDIR)/ref_gl/rw_in_svgalib.o \
	$(BUILDDIR)/ref_gl/gl_fxmesa.o

$(BUILDDIR)/ref_glx.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_GLX_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_GLX_OBJS) $(GLXLDFLAGS)

$(BUILDDIR)/ref_gl.$(SHLIBEXT) : $(REF_GL_OBJS) $(REF_FXGL_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(REF_GL_OBJS) $(REF_FXGL_OBJS) $(FXGLLDFLAGS)

$(BUILDDIR)/ref_gl/gl_draw.o :        $(REF_GL_DIR)/gl_draw.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_image.o :       $(REF_GL_DIR)/gl_image.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_light.o :       $(REF_GL_DIR)/gl_light.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_mesh.o :        $(REF_GL_DIR)/gl_mesh.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_model.o :       $(REF_GL_DIR)/gl_model.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_rmain.o :       $(REF_GL_DIR)/gl_rmain.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_rmisc.o :       $(REF_GL_DIR)/gl_rmisc.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_rsurf.o :       $(REF_GL_DIR)/gl_rsurf.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_warp.o :        $(REF_GL_DIR)/gl_warp.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/qgl_linux.o :      $(LINUX_DIR)/qgl_linux.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/q_shared.o :       $(GAME_DIR)/q_shared.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/q_shlinux.o :      $(LINUX_DIR)/q_shlinux.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/glob.o :           $(LINUX_DIR)/glob.c
	$(DO_GL_SHLIB_CC)

$(BUILDDIR)/ref_gl/gl_glx.o :         $(LINUX_DIR)/gl_glx.c
	$(DO_GL_SHLIB_CC) $(GLXCFLAGS)

$(BUILDDIR)/ref_gl/gl_fxmesa.o :      $(LINUX_DIR)/gl_fxmesa.c
	$(DO_GL_SHLIB_CC) $(FXGLCFLAGS)

$(BUILDDIR)/ref_gl/rw_in_svgalib.o :  $(LINUX_DIR)/rw_in_svgalib.c
	$(DO_GL_SHLIB_CC)

#############################################################################
# MISC
#############################################################################

clean: clean-debug clean-release

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean2:
	rm -f \
	$(QUAKE2_OBJS) \
	$(Q2DED_OBJS) \
	$(QUAKE2_AS_OBJS) \
	$(GAME_OBJS) \
	$(REF_SOFT_OBJS) \
	$(REF_SOFT_SVGA_OBJS) \
	$(REF_SOFT_X11_OBJS) \
	$(REF_GL_OBJS)

distclean:
	@-rm -rf $(BUILD_DEBUG_DIR) $(BUILD_RELEASE_DIR)
	@-rm -f `find . \( -not -type d \) -and \
		\( -name '*~' \) -type f -print`
