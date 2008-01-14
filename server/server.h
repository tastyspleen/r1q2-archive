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
#include "../qcommon/md4.h"
#include "../game/game.h"

//=============================================================================

#define	MAX_MASTERS	8				// max recipients for heartbeat packets

#define	CMDBAN_MESSAGE		0
#define	CMDBAN_KICK			1
#define	CMDBAN_SILENT		2
#define	CMDBAN_BLACKHOLE	3

#define	CMDBAN_LOG_MESSAGE	0
#define	CMDBAN_LOG_SILENT	1

typedef struct bannedcommands_s
{
	struct bannedcommands_s *next;
	char					*name;
	int16					kickmethod;
	int16					logmethod;
} bannedcommands_t;

extern bannedcommands_t bannedcommands;

typedef struct ratelimit_s
{
	int			count;
	int			period;
	uint32		time;
} ratelimit_t;

typedef struct linkednamelist_s
{
	struct linkednamelist_s	*next;
	char					*name;
} linkednamelist_t;

typedef struct linkedvaluelist_s
{
	struct linkedvaluelist_s	*next;
	char						*name;
	char						*value;
} linkedvaluelist_t;

extern linkednamelist_t		nullcmds;
extern linkednamelist_t		lrconcmds;
extern linkedvaluelist_t	serveraliases;

extern	char svConnectStuffString[1100];
extern	char svBeginStuffString[1100];

// some qc commands are only valid before the server has finished
// initializing (precache commands, static sounds / objects, etc)

typedef struct
{
	server_state_t	state;			// precache commands are only valid during load

	qboolean	attractloop;		// running cinematics and demos for the local system only
	qboolean	loadgame;			// client begins should reuse existing entity

	uint32		time;				// always sv.framenum * 100 msec
	int			framenum;

	char		name[MAX_QPATH];			// map name, or cinematic name
	struct cmodel_s		*models[MAX_MODELS];

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	//r1: pointer now (since each client now has their own set) - this avoids
	//having a stupid 17mb [MAX_CLIENTS][MAX_EDICTS] array.
	//entity_state_t	*baselines[MAX_CLIENTS]; //[MAX_EDICTS];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until SV_Multicast is called
	//sizebuf_t	multicast;
	//byte		multicast_buf[MAX_MSGLEN];

	// demo server information
	FILE		*demofile;
	uint32		randomframe;
} server_t;

//qboolean RateLimited (ratelimit_t *limit, int maxCount);
//void RateSample (ratelimit_t *limit);

#define EDICT_NUM(n) ((edict_t *)((byte *)ge->edicts + ge->edict_size*(n)))
#define NUM_FOR_EDICT(e) (int)(( ((byte *)(e)-(byte *)ge->edicts ) / ge->edict_size))


typedef enum
{
	cs_free,		// can be reused for a new connection
	cs_zombie,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	cs_connected,	// has been assigned to a client_t, but not in game yet
	cs_spawning,	// r1: received new, not begin yet.
	cs_spawned		// client is fully in game
} serverclient_state_t;

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

typedef struct
{
	int		msec;
	int		elapsed;
	vec3_t	origin_start;
	vec3_t	origin_end;
	vec3_t	origin_saved;
} pmovestatus_t;

typedef struct client_s
{
	serverclient_state_t	state;

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
	//sizebuf_t		datagram;
	//byte			datagram_buf[MAX_MSGLEN];

	client_frame_t	frames[UPDATE_BACKUP];	// updates can be delta'd from here

	byte			*download;			// file being downloaded
	int				downloadsize;		// total bytes (can't use EOF because of paks)
	int		 		downloadcount;		// bytes sent
	
	//r1: compress downloads?
	qboolean		downloadCompressed;

	char			*downloadFileName;

	int				lastmessage;		// sv.framenum when packet was last received

	uint32	 		challenge;			// challenge of this user, randomly generated

	netchan_t		netchan;
	
	//r1: client protocol
	uint32	 		protocol;
	uint32			protocol_version;

	//r1: number of times they've commandMsec underflowed (if this gets excessive then
	//they can be dropped)
	float			commandMsecOverflowCount;

	//r1: number of consecutive nodelta frames
	uint32			nodeltaframes;

	//r1: don't send game data to this client (bots etc)
	qboolean		nodata;

	//r1: client-specific last deltas (kind of like dynamic baselines)
	entity_state_t	*lastlines;

	//r1: misc flags
	uint32			notes;

	//r1: number of packets received over last 5 seconds
	int							packetCount;

	//r1: estimated FPS
	int							fps;

	//r1: number of frames since last activity
	int							idletime;

	//r1: version string
	char						*versionString;

	char						reconnect_var[32];
	char						reconnect_value[32];
	qboolean					reconnect_done;

	messagelist_t				*messageListData;
	messagelist_t				*msgListStart;
	messagelist_t				*msgListEnd;

	qboolean					moved;

	unsigned long				settings[CLSET_MAX];
	unsigned					totalMsecUsed;
	unsigned					enterFrame;

#ifdef ANTICHEAT
	qboolean					anticheat_valid;
	int							anticheat_query_sent;
	int							anticheat_required;
	int							anticheat_file_failures;
	linkednamelist_t			anticheat_bad_files;
	unsigned					anticheat_query_time;
	unsigned					anticheat_nag_time;
	const char					*anticheat_token;
	int							anticheat_client_type;
#endif
	int							beginspawncount;

	unsigned					pl_dropped_packets;
	unsigned					pl_sent_packets;
	unsigned					pl_last_packet_frame;

	unsigned					min_ping, avg_ping_count, avg_ping_time, max_ping;
	unsigned					last_incoming_sequence;
	unsigned					player_updates_sent;

	pmovestatus_t				current_move;

	char						*cheaternet_message;

	//ugly hack for variable FPS support dropping s.event
	int							entity_events[MAX_EDICTS];

#ifdef _DEBUG
	qboolean					ratbot_hack_pending;
#endif
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
	int32		challenge;
	uint32	 	time;
} challenge_t;

typedef struct
{
	qboolean	initialized;				// sv_init has completed
	int			realtime;					// always increasing, no clamping, etc

	char		mapcmd[MAX_TOKEN_CHARS];	// ie: *intro.cin+base 

	int			spawncount;					// incremented each server start
											// used to check late spawns

	client_t	*clients;					// [maxclients->value];
	uint32 		num_client_entities;		// maxclients->value*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	uint32 		next_client_entities;		// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]

	unsigned	last_playerupdate;

	int			last_heartbeat;

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting

	// serverrecord values
	FILE		*demofile;
	sizebuf_t	demo_multicast;
	byte		demo_multicast_buf[MAX_MSGLEN];

	// rate limit status requests
	ratelimit_t	ratelimit_status;
	ratelimit_t	ratelimit_badrcon;

	//crazy stats :)
#ifndef NPROFILE
	unsigned long		proto35BytesSaved;
	unsigned long		proto35CompressionBytes;
	unsigned long		r1q2OptimizedBytes;
	unsigned long		r1q2CustomBytes;
	unsigned long		r1q2AttnBytes;
#endif
} server_static_t;

extern	cvar_t	*sv_ratelimit_status;

//=============================================================================

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;

extern	netadr_t	cheaternet_adr;

extern	netadr_t	master_adr[MAX_MASTERS];	// address of the master server

extern	server_static_t	svs;				// persistant server info
extern	server_t		sv;					// local server

extern	cvar_t		*sv_paused;
extern	cvar_t		*maxclients;
extern	cvar_t		*sv_noreload;			// don't reload level state when reentering
extern	cvar_t		*sv_airaccelerate;		// don't reload level state when reentering
											// development tool
extern	cvar_t		*sv_max_download_size;
extern	cvar_t		*sv_downloadserver;

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

extern	cvar_t		*sv_mapdownload_denied_message;
extern	cvar_t		*sv_mapdownload_ok_message;

extern	cvar_t		*sv_new_entflags;

extern	cvar_t		*sv_validate_playerskins;

extern	cvar_t		*sv_idlekick;

extern	cvar_t		*sv_entity_inuse_hack;
extern	cvar_t		*sv_force_reconnect;

extern	cvar_t		*timeout;
extern	cvar_t		*sv_download_refuselimit;

extern	cvar_t		*sv_download_drop_file;
extern	cvar_t		*sv_download_drop_message;

extern	cvar_t		*sv_msecs;

extern	cvar_t		*sv_blackhole_mask;
extern	cvar_t		*sv_badcvarcheck;

extern	cvar_t		*sv_rcon_showoutput;

extern	client_t	*sv_client;
extern	edict_t		*sv_player;

extern	cvar_t		*sv_enhanced_setplayer;

extern	cvar_t		*sv_predict_on_lag;
extern	cvar_t		*sv_format_string_hack;

extern	cvar_t		*sv_lag_stats;
extern	cvar_t		*sv_func_plat_hack;
extern	cvar_t		*sv_max_packetdup;

extern	cvar_t		*sv_max_player_updates;

extern	cvar_t		*sv_disconnect_hack;

extern	cvar_t		*sv_interpolated_pmove;

extern	cvar_t		*sv_global_master;

#ifdef _DEBUG
extern	cvar_t		*sv_ratbot_hack;
#endif

#ifdef ANTICHEAT
extern	cvar_t		*sv_require_anticheat;
#endif

extern	cvar_t	*allow_download;
extern	cvar_t	*allow_download_players;
extern	cvar_t	*allow_download_models;
extern	cvar_t	*allow_download_sounds;
extern	cvar_t	*allow_download_maps;
extern	cvar_t	*allow_download_pics;
extern	cvar_t	*allow_download_textures;
extern	cvar_t	*allow_download_others;

//===========================================================

//client->notes

#define NOTE_CLIENT_NOCHEAT 0x1
#define	NOTE_OVERFLOWED		0x2
#define	NOTE_OVERFLOW_DONE	0x4
#define	NOTE_DROPME			0x8

//
// sv_main.c
//
//void SV_FinalMessage (const char *message, qboolean reconnect);
void SV_DropClient (client_t *drop, qboolean notify);
void SV_KickClient (client_t *cl, const char /*@null@*/*reason, const char /*@null@*/*cprintf);

int EXPORT SV_ModelIndex (const char *name);
int EXPORT SV_SoundIndex (const char *name);
int EXPORT SV_ImageIndex (const char *name);

void SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg);

//void SV_ExecuteUserCommand (char *s);
void SV_InitOperatorCommands (void);

void SV_SendServerinfo (client_t *client);
void SV_UserinfoChanged (client_t *cl);

void SV_CleanClient (client_t *drop);

//void SV_UpdateUserinfo (client_t *cl, qboolean notifyGame);

extern cvar_t	*sv_filter_q3names;
extern cvar_t	*sv_filter_userinfo;
extern cvar_t	*sv_filter_stringcmds;

extern cvar_t	*sv_allownodelta;
extern cvar_t	*sv_deny_q2ace;

extern cvar_t	*sv_gamedebug;
extern cvar_t	*sv_packetentities_hack;

extern cvar_t	*sv_optimize_deltas;

extern cvar_t	*sv_cheaternet;
extern cvar_t	*sv_disallow_download_sprites_hack;

extern cvar_t	*sv_fps;

//void Master_Heartbeat (void);
//void Master_Packet (void);

//
// sv_init.c
//
void SV_InitGame (void);
void SV_Map (qboolean attractloop, const char *levelstring, qboolean loadgame);

//qboolean CheckUserInfoFields (char *userinfo);

extern cvar_t *hostname;
extern int server_port;

#ifdef _WIN32
void Sys_UpdateConsoleBuffer (void);
#ifdef DEDICATED_ONLY
void Sys_InstallService(char *servername, char *cmdline);
void Sys_DeleteService (char *servername);
#endif
void Sys_EnableTray (void);
void Sys_DisableTray (void);
void Sys_Minimize (void);
#endif

void Blackhole (netadr_t *from, qboolean isAutomatic, int mask, int method, const char *fmt, ...) __attribute__ ((format (printf, 5, 6)));

//
// sv_phys.c
//
//void SV_PrepWorldFrame (void);

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

//void SV_DemoCompleted (void);
void SV_SendClientMessages (void);

void EXPORT SV_Multicast (vec3_t /*@null@*/origin, multicast_t to);
void EXPORT SV_StartSound (vec3_t origin, edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation, float timeofs);

void SV_ClientPrintf (client_t *cl, int level, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
void SV_BroadcastPrintf (int level, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void SV_BroadcastCommand (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

//sizebuf_t *MSGQueueAlloc (client_t *cl, int size, byte type);
//void SV_AddMessageQueue (client_t *client, int extrabytes);
void SV_AddMessage (client_t *cl, qboolean reliable);
//void SV_AddMessageSingle (client_t *cl, qboolean reliable);

void SV_WriteReliableMessages (client_t *client, int buffSize);

void SV_ClearMessageList (client_t *client);

//
// sv_user.c
//
void SV_Nextserver (void);
void SV_ExecuteClientMessage (client_t *cl);

//
// sv_ccmds.c
//
void SV_ReadLevelFile (void);

//
// sv_ents.c
//
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg);
void SV_RecordDemoMessage (void);
void SV_BuildClientFrame (client_t *client);


void SV_Error (const char *error, ...) __attribute__ ((format (printf, 1, 2)));

//
// sv_game.c
//
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
	blackhole_t		*next;
	uint32			ip;
	uint32			mask;
	int				method;
	char			reason[128];
	ratelimit_t		ratelimit;
};

typedef struct netblock_s netblock_t;

struct netblock_s
{
	netblock_t		*next;
	uint32			ip;
	uint32			mask;
};

#define	BLACKHOLE_SILENT	0
#define	BLACKHOLE_MESSAGE	1

#define CVARBAN_KICK		1
#define CVARBAN_BLACKHOLE	2
#define	CVARBAN_LOGONLY		3
#define	CVARBAN_MESSAGE		4
#define	CVARBAN_STUFF		5
#define	CVARBAN_EXEC		6

extern blackhole_t blackholes;
#ifdef ANTICHEAT
extern netblock_t anticheat_exceptions;
#endif

typedef struct banmatch_s banmatch_t;

struct banmatch_s
{
	banmatch_t	*next;
	char		*matchvalue;
	char		*message;
	int			blockmethod;
};

typedef struct varban_s varban_t;

struct varban_s
{
	varban_t	*next;
	char		*varname;
	banmatch_t	match;
};

extern	varban_t	cvarbans;
extern	varban_t	userinfobans;

qboolean SV_UserInfoBanned (client_t *cl);
void UserinfoBanDrop (const char *key, const banmatch_t *ban, const char *result);
const banmatch_t *SV_CheckUserinfoBans (char *userinfo, char *key);
const banmatch_t *VarBanMatch (varban_t *bans, const char *var, const char *result);

extern	cvar_t	*sv_max_traces_per_frame;
extern	unsigned int		sv_tracecount;

qboolean StringIsNumeric (const char *s);
uint32 CalcMask (int32 bits);

extern netblock_t	blackhole_exceptions;

#ifdef ANTICHEAT
void SV_AntiCheat_WaitForInitialConnect (void);
void SV_AntiCheat_Disconnect (void);
qboolean SV_AntiCheat_Connect (void);
qboolean SV_AntiCheat_IsConnected (void);
qboolean SV_AntiCheat_Disconnect_Client (client_t *cl);
qboolean SV_AntiCheat_QueryClient (client_t *cl);
void SV_AntiCheat_Run (void);
qboolean SV_AntiCheat_Challenge (netadr_t *from, client_t *cl);
const char *SV_AntiCheat_CheckToken (const char *token);
void SV_AntiCheat_UpdatePrefs (cvar_t *v, char *oldVal, char *newVal);

#define ANTICHEATMESSAGE "\220\xe1\xee\xf4\xe9\xe3\xe8\xe5\xe1\xf4\221"

extern	cvar_t	*sv_anticheat_server_address;
extern	cvar_t	*sv_anticheat_error_action;
extern	cvar_t	*sv_anticheat_badfile_action;
extern	cvar_t	*sv_anticheat_badfile_message;
extern	cvar_t	*sv_anticheat_badfile_max;
extern	cvar_t	*sv_anticheat_message;

extern cvar_t	*sv_anticheat_nag_time;
extern cvar_t	*sv_anticheat_nag_message;
extern cvar_t	*sv_anticheat_nag_defer;

extern cvar_t	*sv_anticheat_show_violation_reason;
extern cvar_t	*sv_anticheat_client_disconnect_action;

extern cvar_t	*sv_anticheat_disable_play;
extern cvar_t	*sv_anticheat_client_restrictions;
extern cvar_t	*sv_anticheat_force_protocol35;

extern	int		antiCheatNumFileHashes;

void SVCmd_SVACList_f (void);
void SVCmd_SVACInfo_f (void);
void SVCmd_SVACUpdate_f (void);
void SVCmd_SVACInvalidate_f (void);

enum
{
	ANTICHEAT_NORMAL,
	ANTICHEAT_REQUIRED,
	ANTICHEAT_EXEMPT
};

enum
{
	ANTICHEAT_QUERY_UNSENT,
	ANTICHEAT_QUERY_SENT,
	ANTICHEAT_QUERY_DONE
};

#define ACCLIENT_R1Q2	0x01
#define ACCLIENT_EGL	0x02
#define	ACCLIENT_APRGL	0x04
#define	ACCLIENT_APRSW	0x08
#define	ACCLIENT_Q2PRO	0x10

extern netblock_t	anticheat_exceptions;
extern netblock_t	anticheat_requirements;

extern char anticheat_hashlist_name[256];

extern const char *anticheat_client_names[];

#endif

void SV_ClientBegin (client_t *cl);
void SV_SendPlayerUpdates (int msec_to_next_frame);
