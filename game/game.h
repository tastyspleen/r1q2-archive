/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// game.h -- game dll information visible to server

#ifdef ENHANCED_SERVER
#define	GAME_API_VERSION			4
#else
#define GAME_API_VERSION			3
#endif

//#define	GAME_API_VERSION_ENHANCED	4
// edict->svflags

#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_DEADMONSTER			0x00000002	// treat as CONTENTS_DEADMONSTER for collision
#define	SVF_MONSTER				0x00000004	// treat as CONTENTS_MONSTER for collision
#define	SVF_NOPREDICTION		0x00000008	// send this as solid=0 to the client to ignore prediction

// edict->solid values

typedef enum
{
SOLID_NOT,			// no interaction with other objects
SOLID_TRIGGER,		// only touch when inside, after moving
SOLID_BBOX,			// touch on edge
SOLID_BSP			// bsp clip, touch on edge
} solid_t;

//===============================================================

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;

#define	MAX_ENT_CLUSTERS	16


typedef struct edict_s edict_t;

#ifndef GAME_INCLUDE

struct gclient_old_s
{
	player_state_old	ps;		// communicated by server to clients
	int					ping;
	// the game dll can add anything it wants after
	// this point in the structure
};

struct gclient_new_s
{
	player_state_new	ps;		// communicated by server to clients
	int					ping;
	// the game dll can add anything it wants after
	// this point in the structure
};

struct edict_s
{
	entity_state_t	s;
	void		*client;
	qboolean	inuse;
	int			linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t		area;				// linked to a division node or leaf
	
	int			num_clusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			headnode;			// unused if num_clusters != -1
	int			areanum, areanum2;

	//================================

	int			svflags;			// SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;
	solid_t		solid;
	int			clipmask;
	edict_t		*owner;

	// the game dll can add anything it wants after
	// this point in the structure
};

#else

typedef player_state_old player_state_t;
typedef struct gclient_s gclient_t;

#endif		// GAME_INCLUDE

//===============================================================

//
// functions provided by the main engine
//
typedef struct
{
	// special messages
	// it seems gcc 2 doesn't like these at all.
#if __GNUC__ > 2
	void	(EXPORT *bprintf) (int printlevel, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
	void	(EXPORT *dprintf) (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
	void	(EXPORT *cprintf) (edict_t *ent, int printlevel, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
	void	(EXPORT *centerprintf) (edict_t *ent, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
#else
	void	(EXPORT *bprintf) (int printlevel, const char *fmt, ...);
	void	(EXPORT *dprintf) (const char *fmt, ...);
	void	(EXPORT *cprintf) (edict_t *ent, int printlevel, const char *fmt, ...);
	void	(EXPORT *centerprintf) (edict_t *ent, const char *fmt, ...);
#endif

	void	(EXPORT *sound) (edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
	void	(EXPORT *positioned_sound) (vec3_t origin, edict_t *ent, int channel, int soundinedex, float volume, float attenuation, float timeofs);

	// config strings hold all the index strings, the lightstyles,
	// and misc data like the sky definition and cdtrack.
	// All of the current configstrings are sent to clients when
	// they connect, and changes are sent to all connected clients.
	void	(EXPORT *configstring) (int num, char *string);

#if __GNUC__ > 2
	void	(EXPORT *error) (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
	void	(EXPORT *error) (const char *fmt, ...);
#endif

	// the *index functions create configstrings and some internal server state
	int		(EXPORT *modelindex) (const char *name);
	int		(EXPORT *soundindex) (const char *name);
	int		(EXPORT *imageindex) (const char *name);

	void	(EXPORT *setmodel) (edict_t *ent, const char *name);

	// collision detection
	trace_t	(EXPORT *trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	int		(EXPORT *pointcontents) (vec3_t point);
	qboolean	(EXPORT *inPVS) (vec3_t p1, vec3_t p2);
	qboolean	(EXPORT *inPHS) (vec3_t p1, vec3_t p2);
	void		(EXPORT *SetAreaPortalState) (int portalnum, qboolean open);
	qboolean	(EXPORT *AreasConnected) (int area1, int area2);

	// an entity will never be sent to a client or used for collision
	// if it is not passed to linkentity.  If the size, position, or
	// solidity changes, it must be relinked.
	void	(EXPORT *linkentity) (edict_t *ent);
	void	(EXPORT *unlinkentity) (edict_t *ent);		// call before removing an interactive edict
	int		(EXPORT *BoxEdicts) (vec3_t mins, vec3_t maxs, edict_t **list,	int maxcount, int areatype);
	void	(EXPORT *Pmove) (pmove_t *pmove);		// player movement code common with client prediction

	// network messaging
	void	(EXPORT *multicast) (vec3_t origin, multicast_t to);
	void	(EXPORT *unicast) (edict_t *ent, qboolean reliable);
	void	(EXPORT *WriteChar) (int c);
	void	(EXPORT *WriteByte) (int c);
	void	(EXPORT *WriteShort) (int c);
	void	(EXPORT *WriteLong) (int c);
	void	(EXPORT *WriteFloat) (float f);
	void	(EXPORT *WriteString) (const char *s);
	void	(EXPORT *WritePosition) (vec3_t pos);	// some fractional bits
	void	(EXPORT *WriteDir) (vec3_t pos);		// single byte encoded, very coarse
	void	(EXPORT *WriteAngle) (float f);

	// managed memory allocation
	void	*(EXPORT *TagMalloc) (int size, int tag);
	void	(EXPORT *TagFree) (void *block);
	void	(EXPORT *FreeTags) (int tag);

	// console variable interaction
	cvar_t	*(EXPORT *cvar) (const char *var_name, const char *value, int flags);
	cvar_t	*(EXPORT *cvar_set) (const char *var_name, const char *value);
	cvar_t	*(EXPORT *cvar_forceset) (const char *var_name, const char *value);

	// ClientCommand and ServerCommand parameter access
	int		(EXPORT *argc) (void);
	char	*(EXPORT *argv) (int n);
	char	*(EXPORT *args) (void);	// concatenation of all argv >= 1

	// add commands to the server console as if they were typed in
	// for map changing, etc
	void	(EXPORT *AddCommandString) (const char *text);

	void	(EXPORT *DebugGraph) (float value, int color);
} game_import_t;

//
// functions exported by the game subsystem
//
typedef struct
{
	int			apiversion;

	// the init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init
	void		(IMPORT *Init) (void);
	void		(IMPORT *Shutdown) (void);

	// each new level entered will cause a call to SpawnEntities
	void		(IMPORT *SpawnEntities) (const char *mapname, const char *entstring, const char *spawnpoint);

	// Read/Write Game is for storing persistant cross level information
	// about the world state and the clients.
	// WriteGame is called every time a level is exited.
	// ReadGame is called on a loadgame.
	void		(IMPORT *WriteGame) (const char *filename, qboolean autosave);
	void		(IMPORT *ReadGame) (const char *filename);

	// ReadLevel is called after the default map information has been
	// loaded with SpawnEntities
	void		(IMPORT *WriteLevel) (const char *filename);
	void		(IMPORT *ReadLevel) (const char *filename);

	qboolean	(IMPORT *ClientConnect) (edict_t *ent, char *userinfo);
	void		(IMPORT *ClientBegin) (edict_t *ent);
	void		(IMPORT *ClientUserinfoChanged) (edict_t *ent, char *userinfo);
	void		(IMPORT *ClientDisconnect) (edict_t *ent);
	void		(IMPORT *ClientCommand) (edict_t *ent);
	void		(IMPORT *ClientThink) (edict_t *ent, usercmd_t *cmd);

	void		(IMPORT *RunFrame) (void);

	// ServerCommand will be called when an "sv <command>" command is issued on the
	// server console.
	// The game can issue gi.argc() / gi.argv() commands to get the rest
	// of the parameters
	void		(IMPORT *ServerCommand) (void);

	//
	// global variables shared between game and server
	//

	// The edict array is allocated in the game dll so it
	// can vary in size from one game to another.
	// 
	// The size will be fixed when ge->Init() is called
	struct edict_s	*edicts;
	int			edict_size;
	int			num_edicts;		// current number, <= max_edicts
	int			max_edicts;
} game_export_t;

game_export_t * IMPORT GetGameApi (game_import_t *import);
