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
// server.h


//define	PARANOID			// speed sapping error checking

#include "../qcommon/qcommon.h"
#include "../game/game.h"

//=============================================================================

#define	MAX_MASTERS	8				// max recipients for heartbeat packets

typedef struct bannedcommands_s
{
	struct bannedcommands_s *next;
	char					*name;
} bannedcommands_t;

extern bannedcommands_t bannedcommands;

typedef struct ratelimit_s
{
	netadr_t	from;
	int			count;
	int			time[16];
} ratelimit_t;

// some qc commands are only valid before the server has finished
// initializing (precache commands, static sounds / objects, etc)

typedef struct
{
	server_state_t	state;			// precache commands are only valid during load

	qboolean	attractloop;		// running cinematics and demos for the local system only
	qboolean	loadgame;			// client begins should reuse existing entity

	unsigned	time;				// always sv.framenum * 100 msec
	int			framenum;

	char		name[MAX_QPATH];			// map name, or cinematic name
	struct cmodel_s		*models[MAX_MODELS];

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	//r1: pointer now (since each client now has their own set) - this avoids
	//having a stupid 17mb [MAX_CLIENTS][MAX_EDICTS] array.
	//entity_state_t	*baselines[MAX_CLIENTS]; //[MAX_EDICTS];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until SV_Multicast is called
	sizebuf_t	multicast;
	byte		multicast_buf[MAX_MSGLEN];

	// demo server information
	FILE		*demofile;
	
	// rate limit status requests
	ratelimit_t	ratelimit_status[16];
} server_t;

#define EDICT_NUM(n) ((edict_t *)((byte *)ge->edicts + ge->edict_size*(n)))
#define NUM_FOR_EDICT(e) ( ((byte *)(e)-(byte *)ge->edicts ) / ge->edict_size)


typedef enum
{
	cs_free,		// can be reused for a new connection
	cs_zombie,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	cs_connected,	// has been assigned to a client_t, but not in game yet
	cs_spawned		// client is fully in game
} client_state_t;

typedef struct
{
	int					areabytes;
	byte				areabits[MAX_MAP_AREAS/8];		// portalarea visibility bits
	player_state_new 	ps;
	int					num_entities;
	int					first_entity;		// into the circular sv_packet_entities[]
	int					senttime;			// for ping calculations
} client_frame_t;

#define	LATENCY_COUNTS	64
#define	RATE_MESSAGES	10

//#define MAX_DELTA_SAMPLES 30

typedef struct message_queue_s
{
	struct message_queue_s	*next;
	byte					type;
	byte					*data;
	sizebuf_t				buf;
	int						queued_frame;
	int						len;
} message_queue_t;

typedef struct client_s
{
	client_state_t	state;

	char			userinfo[MAX_INFO_STRING];		// name, etc

	int				lastframe;			// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops

	int				commandMsec;		// every seconds this is reset, if user
										// commands exhaust it, assume time cheating

	int				frame_latency[LATENCY_COUNTS];
	int				ping;

	int				message_size[RATE_MESSAGES];	// used to rate drop packets
	int				rate;
	int				surpressCount;		// number of messages rate supressed

	edict_t			*edict;				// EDICT_NUM(clientnum+1)
	char			name[16];			// extracted from userinfo, high bits masked
	int				messagelevel;		// for filtering printed messages

	// The datagram is written to by sound calls, prints, temp ents, etc.
	// It can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte			datagram_buf[MAX_MSGLEN];

	client_frame_t	frames[UPDATE_BACKUP];	// updates can be delta'd from here

	byte			*download;			// file being downloaded
	unsigned int				downloadsize;		// total bytes (can't use EOF because of paks)
	unsigned int				downloadcount;		// bytes sent
	unsigned short	downloadport;

	int				lastmessage;		// sv.framenum when packet was last received
	//int				lastconnect;

	unsigned int				challenge;			// challenge of this user, randomly generated

	netchan_t		netchan;
	
	//r1: client protocol
	unsigned int				protocol;

	//r1: number of times they've commandMsec underflowed (if this gets excessive then
	//they can be dropped)
	float						commandMsecOverflowCount;

	//r1: number of consecutive nodelta frames
	unsigned int				nodeltaframes;

	//r1: don't send game data to this client (bots etc)
	qboolean					nodata;

	//r1: client-specificlast deltas (kind of like dynamic baselines)
	entity_state_t				*lastlines;

	//r1: misc flags
	unsigned int				notes;

	//r1: number of packets received over last 5 seconds
	int							packetCount;

	//r1: estimated FPS
	int							fps;

	//r1: version string
	char						*versionString;

	message_queue_t				messageQueue;
} client_t;

// a client can leave the server in one of four ways:
// dropping properly by quiting or disconnecting
// timing out if no valid messages are received for timeout.value seconds
// getting kicked off by the server operator
// a program error, like an overflowed reliable buffer

//=============================================================================

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	1024

typedef struct
{
	netadr_t	adr;
	int			challenge;
	unsigned int			time;
} challenge_t;


typedef struct
{
	qboolean	initialized;				// sv_init has completed
	int			realtime;					// always increasing, no clamping, etc

	char		mapcmd[MAX_TOKEN_CHARS];	// ie: *intro.cin+base 

	int			spawncount;					// incremented each server start
											// used to check late spawns

	client_t	*clients;					// [maxclients->value];
	unsigned int			num_client_entities;		// maxclients->value*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	unsigned int			next_client_entities;		// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]

	int			last_heartbeat;

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting

	// serverrecord values
	FILE		*demofile;
	sizebuf_t	demo_multicast;
	byte		demo_multicast_buf[MAX_MSGLEN];
} server_static_t;

typedef struct dlinfo_s
{
	int	sock;
	client_t	*cl;
} dlinfo_t;

//=============================================================================

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;

extern	netadr_t	master_adr[MAX_MASTERS];	// address of the master server

extern	server_static_t	svs;				// persistant server info
extern	server_t		sv;					// local server

extern	cvar_t		*sv_paused;
extern	cvar_t		*maxclients;
extern	cvar_t		*sv_noreload;			// don't reload level state when reentering
extern	cvar_t		*sv_airaccelerate;		// don't reload level state when reentering
											// development tool
extern	cvar_t		*sv_max_download_size;
extern	cvar_t		*sv_downloadwait;
extern	cvar_t		*sv_downloadport;

extern	cvar_t		*sv_nc_visibilitycheck;
extern	cvar_t		*sv_nc_clientsonly;

extern	cvar_t		*sv_max_netdrop;

extern	cvar_t		*sv_enforcetime;

extern	cvar_t		*sv_randomframe;

extern	cvar_t		*sv_nc_kick;
extern	cvar_t		*sv_nc_announce;
extern	cvar_t		*sv_filter_nocheat_spam;

extern	cvar_t		*sv_recycle;
extern	cvar_t		*sv_strafejump_hack;

extern	cvar_t		*sv_allow_map;
extern	cvar_t		*sv_allow_unconnected_cmds;

extern	client_t	*sv_client;
extern	edict_t		*sv_player;

extern	cvar_t	*allow_download;
extern	cvar_t	*allow_download_players;
extern	cvar_t	*allow_download_models;
extern	cvar_t	*allow_download_sounds;
extern	cvar_t	*allow_download_maps;

//===========================================================

#define CLIENT_NOCHEAT 0x1

//
// sv_main.c
//
void SV_FinalMessage (char *message, qboolean reconnect);
void SV_DropClient (client_t *drop);
void SV_KickClient (client_t *cl, char /*@null@*/*reason, char /*@null@*/*cprintf);

int EXPORT SV_ModelIndex (char *name);
int EXPORT SV_SoundIndex (char *name);
int EXPORT SV_ImageIndex (char *name);

void SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg);

void SV_ExecuteUserCommand (char *s);
void SV_InitOperatorCommands (void);

void SV_SendServerinfo (client_t *client);
void SV_UserinfoChanged (client_t *cl);

extern cvar_t	*sv_filter_q3names;
extern cvar_t	*sv_filter_userinfo;
extern cvar_t	*sv_filter_stringcmds;

extern cvar_t	*sv_allownodelta;
extern cvar_t	*sv_deny_q2ace;

extern cvar_t	*sv_gamedebug;

void Master_Heartbeat (void);
void Master_Packet (void);

//
// sv_init.c
//
void SV_InitGame (void);
void SV_Map (qboolean attractloop, char *levelstring, qboolean loadgame);

qboolean CheckUserInfoFields (char *userinfo);

extern cvar_t *hostname;
extern int server_port;

#ifdef WIN32
void Sys_SetWindowText(char *buff);
void Sys_UpdateConsoleBuffer (void);
void Sys_InstallService(char *servername, char *cmdline);
void Sys_DeleteService (char *servername);
void Sys_EnableTray (void);
void Sys_DisableTray (void);
void Sys_Minimize (void);
#endif

void Blackhole (netadr_t *from, char *fmt, ...);

//
// sv_phys.c
//
void SV_PrepWorldFrame (void);

//
// sv_send.c
//
typedef enum {RD_NONE, RD_CLIENT, RD_PACKET} redirect_t;

//r1: since this is only used for rcon now, why not throw the data into one
//    huge damn packet (avoids reassembly issues on client putting the data
//    in the wrong order (smaller packets arrive faster etc))
#define	SV_OUTPUTBUF_LENGTH	(MAX_MSGLEN - 16)

extern	char	sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect (int sv_redirected, char *outputbuf);

void SV_DemoCompleted (void);
void SV_SendClientMessages (void);

void EXPORT SV_Multicast (vec3_t /*@null@*/origin, multicast_t to);
void EXPORT SV_StartSound (vec3_t origin, edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation, float timeofs);
void SV_ClientPrintf (client_t *cl, int level, char *fmt, ...);
void SV_BroadcastPrintf (int level, char *fmt, ...);
void SV_BroadcastCommand (char *fmt, ...);

//
// sv_user.c
//
void SV_Nextserver (void);
void SV_ExecuteClientMessage (client_t *cl);

//
// sv_ccmds.c
//
void SV_ReadLevelFile (void);
void SV_Status_f (void);

//
// sv_ents.c
//
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg);
void SV_RecordDemoMessage (void);
void SV_BuildClientFrame (client_t *client);


void SV_Error (char *error, ...);

//
// sv_game.c
//
extern	int	sv_download_socket;
extern	game_export_t	*ge;

qboolean EXPORT PF_inPVS (vec3_t p1, vec3_t p2);
qboolean EXPORT PF_inPHS (vec3_t p1, vec3_t p2);

void SV_InitGameProgs (void);
void SV_ShutdownGameProgs (void);
void SV_InitEdict (edict_t *e);



//============================================================

//
// high level object sorting to reduce interaction tests
//

void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void EXPORT SV_UnlinkEdict (edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void EXPORT SV_LinkEdict (edict_t *ent);
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid

int EXPORT SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **list, int maxcount, int areatype);
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?

//===================================================================

//
// functions that interact with everything apropriate
//
int EXPORT SV_PointContents (vec3_t p);
// returns the CONTENTS_* value from the world at the given point.
// Quake 2 extends this to also check entities, to allow moving liquids


trace_t EXPORT SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask);
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks (normally NULL)

void Sys_InitDlMutex (void);
void Sys_FreeDlMutex (void);
void Sys_AcquireDlMutex (void);
void Sys_ReleaseDlMutex (void);

//r1: blackholes
typedef struct blackhole_s blackhole_t;

struct blackhole_s
{
	blackhole_t	*next;
	netadr_t	netadr;
	char		reason[128];
};

#define CVARBAN_KICK		1
#define CVARBAN_BLACKHOLE	2

extern blackhole_t blackholes;

typedef struct banmatch_s banmatch_t;

struct banmatch_s
{
	banmatch_t	*next;
	char		*matchvalue;
	char		*message;
	int			blockmethod;
};

typedef struct cvarban_s cvarban_t;

struct cvarban_s
{
	cvarban_t	*next;
	char		*cvarname;
	banmatch_t	match;
};

extern cvarban_t cvarbans;
