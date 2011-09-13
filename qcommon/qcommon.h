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

// qcommon.h -- definitions common between client and server, but not game.dll

#ifndef _QCOMMON_H

#ifdef NDEBUG
	#include "../build.h"
	#define	VERSION		"b"BUILD
#else
	#define BUILD "DEBUG BUILD"
	#define	VERSION		BUILD
#endif

#include "../game/q_shared.h"

#define	BASEDIRNAME	"baseq2"

#ifdef _WIN32
	#ifdef _WIN64
		#ifdef NDEBUG
			#define BUILDSTRING "Win64 RELEASE"
		#else
			#define BUILDSTRING "Win64 DEBUG"
		#endif
	#else
		#ifdef NDEBUG
			#define BUILDSTRING "Win32 RELEASE"
		#else
			#define BUILDSTRING "Win32 DEBUG"
		#endif
	#endif

	#ifdef _M_AMD64 
		#define	CPUSTRING	"AMD64"
	#elif defined _M_IX86
		#define	CPUSTRING	"x86"
	#elif defined _M_IA64
		#define	CPUSTRING	"IA64"
	#elif defined _M_ALPHA
		#define	CPUSTRING	"AXP"
	#endif

#elif defined __linux__

	#define BUILDSTRING "Linux"
	#define __cdecl

	#ifdef __i386__
		#define CPUSTRING "i386"
	#elif defined __alpha__
		#define CPUSTRING "axp"
	#elif defined __x86_64__
		#define CPUSTRING "x86-64"
	#else
		#define CPUSTRING "Unknown"
	#endif

#elif defined __sun__

	#define BUILDSTRING "Solaris"

	#ifdef __i386__
		#define CPUSTRING "i386"
	#else
		#define CPUSTRING "sparc"
	#endif

#elif defined __FreeBSD__

#define BUILDSTRING "FreeBSD"

	#ifdef __i386__
		#define CPUSTRING "i386"
	#elif defined __x86_64__
		#define CPUSTRING "x86_64"
	#else
		#define CPUSTRING "Unknown"
	#endif

#else	// !WIN32

	#error Unknown architecture, please update qcommon.h!

#endif

//Please don't define this on your own builds, it's used
//in the SIGSEGV handler to help me determine build info
//from crash reports. Users will never see this.
#ifdef R1RELEASE
	#define RELEASESTRING "Binary Build (" __DATE__ ")"
	#if R1RELEASE == 1
		#define R1BINARY "r1q2ded"
	#elif R1RELEASE == 2
		#define R1BINARY "r1q2ded-old"
	#elif R1RELEASE == 3
		#define R1BINARY "r1q2ded-x86_64"
	#elif R1RELEASE == 4
		#define R1BINARY "DEDICATED-x86"
	#elif R1RELEASE == 5
		#define R1BINARY "R1Q2-x86"
	#elif R1RELEASE == 6
		#define R1BINARY "R1Q2-AMD64"
	#elif R1RELEASE == 7
		#define R1BINARY "R1Q2-x86-debug"
	#else
		#error What the hell is going on here
	#endif
	#define PRODUCTNAME "R1Q2"
	#define PRODUCTNAMELOWER "r1q2"
#else
	#define R1BINARY "R1Q2"
	#define RELEASESTRING "Source Build"
	#define PRODUCTNAME "R1Q2 (modified)"
	#define PRODUCTNAMELOWER "r1q2 (mod)"
#endif

#define R1Q2_VERSION_STRING "R1Q2 " VERSION " " CPUSTRING " " __DATE__ " " BUILDSTRING

//============================================================================

//maximum length of message list. keep in mind unreliable messages are
//removed every frame, and that any unreliable write will likely be at least 2+
//bytes, so this number represents the maximum number of reliable messages
//that are allowed to be pending at once. if this is exceeded, the client is
//dropped with an overflow.

#define	MAX_MESSAGES_PER_LIST		2048

//don't forget to pad for struct alignment (rest of struct = 1
#define	MSG_MAX_SIZE_BEFORE_MALLOC	69

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Com_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte		*data;
	int			maxsize;
	int			cursize;
	int			readcount;
	int			buffsize;
} sizebuf_t;

typedef struct messagelist_s
{
	//pointer to the message data - this is either localbuff or heap
	byte					*data;

	//next in list
	struct messagelist_s	*next;

	//message length
	int16					cursize;

	//is it reliable?
	byte					reliable;

	//local buffer to avoid lots of mallocs with small size
	byte					localbuff[MSG_MAX_SIZE_BEFORE_MALLOC];
} messagelist_t;

void SZ_Init (sizebuf_t /*@out@*/*buf, byte /*@out@*/*data, int length);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, const void *data, int length);
void SZ_Print (sizebuf_t *buf, const char *data);	// strcats onto the sizebuf

//============================================================================

//struct usercmd_s;
//struct entity_state_s;

extern	entity_state_t	null_entity_state;
extern	usercmd_t		null_usercmd;
extern	cvar_t			uninitialized_cvar;

void MSG_WriteChar (int c);
void MSG_BeginWriting (int c);
void MSG_WriteByte (int c);
void MSG_WriteShort (int c);
void MSG_WriteLong (int c);
void MSG_WriteFloat (float f);
void MSG_WriteString (const char *s);
void MSG_WriteCoord (float f);
void MSG_WritePos (vec3_t pos);
void MSG_WriteAngle (float f);
void MSG_WriteAngle16 (float f);
void MSG_EndWriting (sizebuf_t *out);
void MSG_EndWrite (messagelist_t *out);
void MSG_Write (const void *data, int length);
void MSG_Print (const char *data);

int	MSG_GetLength (void);
byte *MSG_GetData (void);
sizebuf_t *MSG_GetRawMsg (void);
byte MSG_GetType (void);
void MSG_FreeData (void);
void MSG_Clear(void);

void SZ_WriteByte (sizebuf_t *buf, int c);
void SZ_WriteShort (sizebuf_t *buf, int c);
void SZ_WriteLong (sizebuf_t *buf, int c);


void MSG_WriteDeltaUsercmd (const struct usercmd_s *from, const struct usercmd_s /*@out@*/*cmd, int protocol);
void MSG_WriteDir (vec3_t vector);

void SV_WriteDeltaEntity (const struct entity_state_s *from, const struct entity_state_s /*@out@*/*to, qboolean force, qboolean newentity, int cl_protocol, int protocol_version);


void	MSG_BeginReading (sizebuf_t *sb);
int		MSG_ReadChar (sizebuf_t *sb);
int		MSG_ReadByte (sizebuf_t *sb);
int		MSG_ReadShort (sizebuf_t *sb);
int		MSG_ReadLong (sizebuf_t *sb);
float	MSG_ReadFloat (sizebuf_t *sb);
char	*MSG_ReadString (sizebuf_t *sb);
char	*MSG_ReadStringLine (sizebuf_t *sb);

float	MSG_ReadCoord (sizebuf_t *sb);
void	MSG_ReadPos (sizebuf_t *sb, vec3_t pos);
float	MSG_ReadAngle (sizebuf_t *sb);
float	MSG_ReadAngle16 (sizebuf_t *sb);
void	MSG_ReadDir (sizebuf_t *sb, vec3_t vector);

void	MSG_ReadData (sizebuf_t *sb, void *buffer, int size);

void	MSG_ReadDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s /*@out@*/ *cmd, int protocol);

void Q_NullFunc(void);

//============================================================================

#if Q_BIGENDIAN
extern	qboolean		bigendien;

extern	int16	LittleShort (int16 l);
extern	int32		LittleLong (int32 l);
extern	float	LittleFloat (float l);
#endif
//============================================================================

extern	qboolean	q2_initialized;

int	COM_Argc (void);
char *COM_Argv (int arg);	// range and null checked
void COM_ClearArgv (int arg);
//int COM_CheckParm (char *parm);
void COM_AddParm (char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, char **argv);

char *CopyString (const char *in, int tag);

void StripHighBits (char *string, int highbits);
void ExpandNewLines (char *string);
const char *MakePrintable (const void *s, size_t numchars);
qboolean isvalidchar (int c);

//============================================================================

void Info_Print (const char *s);


/* crc.h */

void CRC_Init(uint16 /*@out@*/*crcvalue);
void CRC_ProcessByte(uint16 *crcvalue, byte data);
uint16 CRC_Value(uint16 crcvalue);
uint16 CRC_Block (byte *start, int count);


/*
==============================================================

PROTOCOL

==============================================================
*/

// protocol.h -- communications protocols

#define	PROTOCOL_ORIGINAL	34
#define	PROTOCOL_R1Q2		35

#define	MINOR_VERSION_R1Q2				1905

//minimum versions for some features
#define MINOR_VERSION_R1Q2_UCMD_UPDATES	1904
#define	MINOR_VERSION_R1Q2_32BIT_SOLID	1905

//=========================================

#define	PORT_MASTER	27900
//#define	PORT_CLIENT	27901
#define	PORT_SERVER	27910

//=========================================

#define	UPDATE_BACKUP	16	// copies of entity_state_t to keep buffered
							// must be power of two
#define	UPDATE_MASK		(UPDATE_BACKUP-1)

#define NET_NONE		0
#define NET_CLIENT		1
#define NET_SERVER		2


//==================
// the svc_strings[] array in cl_parse.c should mirror this
//==================

extern	char *svc_strings[];

//
// server to client
//
enum svc_ops_e
{
	svc_bad,

	// these ops are known to the game dll
	svc_muzzleflash,
	svc_muzzleflash2,
	svc_temp_entity,
	svc_layout,
	svc_inventory,

	// the rest are private to the client and server
	svc_nop,
	svc_disconnect,
	svc_reconnect,
	svc_sound,					// <see code>
	svc_print,					// [byte] id [string] null terminated string
	svc_stufftext,				// [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,				// [long] protocol ...
	svc_configstring,			// [short] [string]
	svc_spawnbaseline,		
	svc_centerprint,			// [string] to put in center of the screen
	svc_download,				// [short] size [size bytes]
	svc_playerinfo,				// variable
	svc_packetentities,			// [...]
	svc_deltapacketentities,	// [...]
	svc_frame,

	// ********** r1q2 specific ***********
	svc_zpacket,
	svc_zdownload,
	svc_playerupdate,
	svc_setting,
	// ********** end r1q2 specific *******

	svc_max_enttypes
};

typedef enum
{
	CLSET_NOGUN,
	CLSET_NOBLEND,
	CLSET_RECORDING,
	CLSET_PLAYERUPDATE_REQUESTS,
	CLSET_FPS,
	CLSET_MAX
} clientsetting_t;

typedef enum
{
	SVSET_PLAYERUPDATES,
	SVSET_FPS,
	SVSET_MAX
} serversetting_t;

//==============================================

//
// client to server
//
enum clc_ops_e
{
	clc_bad,
	clc_nop, 		
	clc_move,				// [[usercmd_t]
	clc_userinfo,			// [[userinfo string]
	clc_stringcmd,			// [string] message
	clc_setting,			// [setting][value] R1Q2 settings support.
	clc_multimoves
};

//==============================================

// player_state_t communication

#define	PS_M_TYPE			(1<<0)
#define	PS_M_ORIGIN			(1<<1)
#define	PS_M_VELOCITY		(1<<2)
#define	PS_M_TIME			(1<<3)
#define	PS_M_FLAGS			(1<<4)
#define	PS_M_GRAVITY		(1<<5)
#define	PS_M_DELTA_ANGLES	(1<<6)

#define	PS_VIEWOFFSET		(1<<7)
#define	PS_VIEWANGLES		(1<<8)
#define	PS_KICKANGLES		(1<<9)
#define	PS_BLEND			(1<<10)
#define	PS_FOV				(1<<11)
#define	PS_WEAPONINDEX		(1<<12)
#define	PS_WEAPONFRAME		(1<<13)
#define	PS_RDFLAGS			(1<<14)
#define	PS_BBOX				(1<<15)

//r1 extra hacky bits that are hijacked for more bandwidth goodness. 4 bits in surpresscount
//and 3 in the server message byte (!!!!!!!!)
#define	EPS_GUNOFFSET		(1<<0)
#define	EPS_GUNANGLES		(1<<1)
#define	EPS_PMOVE_VELOCITY2	(1<<2)
#define	EPS_PMOVE_ORIGIN2	(1<<3)
#define	EPS_VIEWANGLE2		(1<<4)
#define	EPS_STATS			(1<<5)

//==============================================

// user_cmd_t communication

// ms and light always sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE2 	(1<<1)
#define	CM_ANGLE3 	(1<<2)
#define	CM_FORWARD	(1<<3)
#define	CM_SIDE		(1<<4)
#define	CM_UP		(1<<5)
#define	CM_BUTTONS	(1<<6)
#define	CM_IMPULSE	(1<<7)

//==============================================

// a sound without an ent or pos will be a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define	SND_POS			(1<<2)		// three coordinates
#define	SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define	SND_OFFSET		(1<<4)		// a byte, msec offset from frame start

#define DEFAULT_SOUND_PACKET_VOLUME	1.0f
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0f

//==============================================

// entity_state_t communication

// try to pack the common update flags into the first byte
#define	U_ORIGIN1	(1<<0)
#define	U_ORIGIN2	(1<<1)
#define	U_ANGLE2	(1<<2)
#define	U_ANGLE3	(1<<3)
#define	U_FRAME8	(1<<4)		// frame is a byte
#define	U_EVENT		(1<<5)
#define	U_REMOVE	(1<<6)		// REMOVE this entity, don't add it
#define	U_MOREBITS1	(1<<7)		// read one additional byte

// second byte
#define	U_NUMBER16	(1<<8)		// NUMBER8 is implicit if not set
#define	U_ORIGIN3	(1<<9)
#define	U_ANGLE1	(1<<10)
#define	U_MODEL		(1<<11)
#define U_RENDERFX8	(1<<12)		// fullbright, etc
#define	U_EFFECTS8	(1<<14)		// autorotate, trails, etc
#define	U_MOREBITS2	(1<<15)		// read one additional byte

// third byte
#define	U_SKIN8		(1<<16)
#define	U_FRAME16	(1<<17)		// frame is a short
#define	U_RENDERFX16 (1<<18)	// 8 + 16 = 32
#define	U_EFFECTS16	(1<<19)		// 8 + 16 = 32
#define	U_MODEL2	(1<<20)		// weapons, flags, etc
#define	U_MODEL3	(1<<21)
#define	U_MODEL4	(1<<22)
#define	U_MOREBITS3	(1<<23)		// read one additional byte

// fourth byte
#define	U_OLDORIGIN	(1<<24)		// FIXME: get rid of this
#define	U_SKIN16	(1<<25)
#define	U_SOUND		(1<<26)
#define	U_SOLID		(1<<27)

//#define	U_COPYOLD	(1<<29)
/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

extern unsigned long r1q2DeltaOptimizedBytes;
extern unsigned long r1q2UserCmdOptimizedBytes;

#define	EXEC_NOW	0		// don't return until completed
#define	EXEC_INSERT	1		// insert at current position, but don't run yet
#define	EXEC_APPEND	2		// add to end of the command buffer

void Cbuf_Init (void);
// allocates an initial text buffer that will grow as needed

void EXPORT Cbuf_AddText (const char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText (const char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void EXPORT Cbuf_ExecuteText (int exec_when, char *text);
// this can be used in place of either Cbuf_AddText or Cbuf_InsertText

void Cbuf_AddEarlyCommands (qboolean clear);
// adds all the +set commands from the command line

qboolean Cbuf_AddLateCommands (void);
// adds all the remaining + commands from the command line
// Returns true if any late commands were added, which
// will keep the demoloop from immediately starting

void Cbuf_Execute (void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

//void Cbuf_CopyToDefer (void);
//void Cbuf_InsertFromDefer (void);
// These two functions are used to defer any pending commands while a map
// is being loaded

//===========================================================================

//r1: zlib
#ifndef NO_ZLIB
int ZLibCompressChunk(byte *in, int len_in, byte *out, int len_out, int method, int wbits);
int ZLibDecompress (byte *in, int inlen, byte /*@out@*/*out, int outlen, int wbits);
#endif
/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

void	Cmd_Init (void);

void	EXPORT Cmd_AddCommand (const char *cmd_name, xcommand_t function);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally
void	EXPORT Cmd_RemoveCommand (const char *cmd_name);

//qboolean Cmd_Exists (char *cmd_name);
// used by the cvar code to check for cvar / command name overlap

const char 	*Cmd_CompleteCommand (const char *partial);
const char 	*Cmd_CompleteCommandOld (const char *partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

int		EXPORT Cmd_Argc (void);
char	* EXPORT Cmd_Argv (int arg);
char	* EXPORT Cmd_Args (void);

//r1: added this
char	*Cmd_Args2 (int arg);

// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

char /*@null@*/ *Cmd_MacroExpandString (char *text);
void	Cmd_TokenizeString (char *text, qboolean macroExpand);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void	Cmd_ExecuteString (char *text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void	Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void Cmd_ExecTrigger (char *string);

/*
==============================================================

CVAR

==============================================================
*/

/*

cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

extern	cvar_t	*cvar_vars;

cvar_t *Cvar_FindVar (const char *var_name);

cvar_t * EXPORT Cvar_Get (const char *var_name, const char *value, int flags);
cvar_t * EXPORT Cvar_GameGet (const char *var_name, const char *var_value, int flags);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t 	* EXPORT Cvar_Set (const char *var_name, const char *value);
// will create the variable if it doesn't exist

cvar_t * EXPORT Cvar_ForceSet (const char *var_name, const char *value);
// will set the variable even if NOSET or LATCH

cvar_t 	*Cvar_FullSet (const char *var_name, const char *value, int flags);

void	EXPORT Cvar_SetValue (const char *var_name, float value);
// expands value to a string and calls Cvar_Set

float	Cvar_VariableValue (const char *var_name);
// returns 0 if not defined or non numeric

int Cvar_IntValue (const char *var_name);

const char	*Cvar_VariableString (const char *var_name);
// returns an empty string if not defined

const char 	*Cvar_CompleteVariable (const char *partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits

void	Cvar_GetLatchedVars (void);
// any CVAR_LATCHED variables that have been set will now take effect

int		Cvar_GetNumLatchedVars (void);
// r1: returns number of latched cvars waiting to be updated

qboolean Cvar_Command (void);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void 	Cvar_WriteVariables (const char *path);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void	Cvar_Init (void);

char	*Cvar_Userinfo (void);
// returns an info string containing all the CVAR_USERINFO cvars

char	*Cvar_Serverinfo (void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern	qboolean	userinfo_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

/*
==============================================================

NET

==============================================================
*/

// net.h -- quake's interface to the networking layer

#define	PORT_ANY	-1

//#define	MAX_MSGLEN		1400	// max length of a message
#define	MAX_MSGLEN		4096		// udp fragmentation isn't so bad these days
#define	PACKET_HEADER	10			// two ints and a short
#define	MAX_USABLEMSG	MAX_MSGLEN - PACKET_HEADER

typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

typedef struct
{
	netadrtype_t	type;

	byte	ip[4];
	//byte	ipx[10];

	uint16	port;
} netadr_t;

void		NET_Init (void);
void		NET_Shutdown (void);

int			NET_Config (int openFlags);

int			NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message);
int			NET_SendPacket (netsrc_t sock, int length, const void *data, netadr_t *to);

#define NET_IsLocalAddress(x) \
	((x)->ip[0] == 127)

#define NET_IsLANAddress(x) \
	(((x)->ip[0] == 127) || ((x)->ip[0] == 10) || (*(uint16 *)(x)->ip == 0xA8C0) || (*(uint16 *)(x)->ip == 0x10AC))
//		127.x.x.x				10.x.x.x					192.168.x.x									172.16.x.x

#define NET_IsLocalHost(x) \
	((x)->type == NA_LOOPBACK)

#define NET_CompareAdr(a,b) \
	((*(uint32 *)(a)->ip == *(uint32 *)(b)->ip) && (a)->port == (b)->port)

#define NET_CompareBaseAdr(a,b) \
	(*(uint32 *)(a)->ip == *(uint32 *)(b)->ip)

char		*NET_inet_ntoa (uint32 ip);
char		*NET_AdrToString (netadr_t *a);
qboolean	NET_StringToAdr (const char *s, netadr_t *a);
#ifndef NO_SERVER
void		NET_Sleep(int msec);
#endif
int NET_Client_Sleep (int msec);
void NET_SetProxy (netadr_t *proxy);

uint32 NET_htonl (uint32 ip);
uint32 NET_ntohl (uint32 ip);

//============================================================================

//#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

typedef struct
{
	//qboolean	fatal_error;
	qboolean	got_reliable;

	netsrc_t	sock;

	int			dropped;			// between last packet and previous

	unsigned	last_received;		// for timeouts
	unsigned	last_sent;			// for retransmits

	netadr_t	remote_address;

	uint16		qport;				// qport value to write when transmitting
	uint16		protocol;

// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	byte		message_buf[MAX_USABLEMSG];		// leave space for header

// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte		reliable_buf[MAX_USABLEMSG];	// unacked reliable message

	unsigned	total_dropped;
	unsigned	total_received;
	unsigned	packetdup;
} netchan_t;

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;
extern	byte		net_message_buffer[MAX_MSGLEN];


void Netchan_Init (void);
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t *adr, int protocol, int qport, unsigned msglen);

qboolean Netchan_NeedReliable (netchan_t *chan);
int	 Netchan_Transmit (netchan_t *chan, int length, const byte /*@null@*/*data);
void Netchan_OutOfBand (int net_socket, netadr_t *adr, int length, const byte *data);
void Netchan_OutOfBandPrint (int net_socket, netadr_t *adr, const char *format, ...);
void Netchan_OutOfBandProxy (int net_socket, netadr_t *adr, int length, const byte *data);
void Netchan_OutOfBandProxyPrint (int net_socket, netadr_t *adr, const char *format, ...);
qboolean Netchan_Process (netchan_t *chan, sizebuf_t *msg);

//qboolean Netchan_CanReliable (netchan_t *chan);


/*
==============================================================

CMODEL

==============================================================
*/


#include "../qcommon/qfiles.h"

cmodel_t	*CM_LoadMap (const char *name, qboolean clientload, uint32 *checksum);
cmodel_t	*CM_InlineModel (const char *name);	// *1, *2, etc

//extern int			CM_NumClusters (void);
//extern int			CM_NumInlineModels (void);
char		*CM_EntityString (void);

// creates a clipping hull for an arbitrary box
int			CM_HeadnodeForBox (const vec3_t mins, const vec3_t maxs);


// returns an ORed contents mask
int			CM_PointContents (const vec3_t p, int headnode);
int			CM_TransformedPointContents (vec3_t p, int headnode, vec3_t origin, vec3_t angles);

trace_t		CM_BoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  int headnode, int brushmask);
trace_t		CM_TransformedBoxTrace (vec3_t start, vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  int headnode, int brushmask,
						  vec3_t origin, vec3_t angles);

byte		*CM_ClusterPVS (int cluster);
byte		*CM_ClusterPHS (int cluster);

int			CM_PointLeafnum (const vec3_t p);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int			CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list,
							int listsize, int *topnode);

const char	*CM_MapName (void);
qboolean	CM_MapWillLoad (const char *name);

//int			CM_LeafContents (int leafnum);
//extern int			CM_LeafCluster (int leafnum);
//extern int			CM_LeafArea (int leafnum);


/*__inline int	CM_NumClusters (void)
{
	return numclusters;
}

__inline int	CM_NumInlineModels (void)
{
	return numcmodels;
*/

typedef struct
{
	int			contents;
	int			cluster;
	int			area;
	uint16		firstleafbrush;
	uint16		numleafbrushes;
} cleaf_t;

extern	int			numclusters;
extern	int			numcmodels;
extern	cleaf_t		map_leafs[MAX_MAP_LEAFS];

#define	CM_NumClusters	(numclusters)
#define	CM_NumInlineModels	(numcmodels)
#define CM_LeafCluster(x) (map_leafs[x].cluster)
#define CM_LeafArea(x) (map_leafs[x].area)

void		EXPORT CM_SetAreaPortalState (int portalnum, qboolean open);
qboolean	EXPORT CM_AreasConnected (int area1, int area2);

int			CM_WriteAreaBits (byte *buffer, int area);
qboolean	CM_HeadnodeVisible (int headnode, const byte *visbits);

void		CM_WritePortalState (FILE *f);
void		CM_ReadPortalState (FILE *f);

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/

extern qboolean pm_airaccelerate;

void Pmove (pmove_new_t *pmove);

/*
==============================================================

FILESYSTEM

==============================================================
*/

typedef enum
{
	HANDLE_NONE,
	HANDLE_OPEN,
	HANDLE_DUPE
} handlestyle_t;

void FS_ReloadPAKs (void);
void	FS_InitFilesystem (void);
void	FS_SetGamedir (const char *dir);
char	*EXPORT FS_Gamedir (void);
char	*FS_NextPath (const char *prevpath);
void	FS_ExecConfig (const char *filename);

qboolean FS_ExistsInGameDir (char *filename);

int		EXPORT FS_FOpenFile (const char *filename, FILE /*@out@*/**file, handlestyle_t openHandle, qboolean *closeHandle);
void	EXPORT FS_FCloseFile (FILE *f);
// note: this can't be called from another DLL, due to MS libc issues

void FS_FlushCache (void);
int		EXPORT FS_LoadFile (const char *path, void /*@out@*/ /*@null@*/**buffer);
// a null buffer will just return the file length without loading
// a -1 length is not present

void	EXPORT FS_Read (void *buffer, int len, FILE *f);
// properly handles partial reads

void	EXPORT FS_FreeFile (void *buffer);

void	FS_CreatePath (char *path);

int		Sys_FileLength (const char *path);
qboolean Sys_CheckFPUStatus (void);
//void Sys_ShellExec (const char *cmd);
void Sys_OpenURL (void);
void Sys_UpdateURLMenu (const char *s);
/*
==============================================================

MISC

==============================================================
*/


//r1: use variadic macros where possible to avoid overhead of evaluations and va
#if __STDC_VERSION__ == 199901L || _MSC_VER >= 1400 && !defined _M_AMD64
#define		Com_DPrintf(...)	\
do { \
	if (developer->intvalue) \
		_Com_DPrintf (__VA_ARGS__); \
} while (0)
#else
#define Com_DPrintf _Com_DPrintf
#endif

void		Com_BeginRedirect (int target, char *buffer, int buffersize, void (*flush));
void		Com_EndRedirect (qboolean flush);
void 		_Com_DPrintf (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void 		Com_Printf (const char *fmt, int level, ...) __attribute__ ((format (printf, 1, 3)));
void 		Com_Error (int code, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void 		Com_Quit (void);

//extern __inline int			Com_ServerState (void);		// this should have just been a cvar...
//extern __inline void		Com_SetServerState (int state);


//r1: why the hell does inline never work on MSVC...
extern	int	server_state;

#define	Com_SetServerState(state) (server_state = state)
#define	Com_ServerState()	(server_state)

uint32	Com_BlockChecksum (void *buffer, int length);
byte	COM_BlockSequenceCRCByte (byte *base, int length, int sequence);

//float	frand(void);	// 0 ti 1
//float	crand(void);	// -1 to 1

extern	cvar_t	*developer;
#ifndef NO_SERVER
extern	cvar_t	*dedicated;
#endif
extern	cvar_t	*host_speeds;
extern	cvar_t	*log_stats;

//extern	FILE *log_stats_file;

extern char	*binary_name;

extern	cvar_t	*dbg_unload;

// host_speeds times
extern	unsigned int		time_before_game;
extern	unsigned int		time_after_game;
extern	unsigned int		time_before_ref;
extern	unsigned int		time_after_ref;

typedef struct tagmalloc_tag_s
{
	int16		value;
	const char	*name;
	uint32		 allocs;
} tagmalloc_tag_t;

//r1: tagmalloc defines
enum tagmalloc_tags_e
{
	TAGMALLOC_NOT_TAGGED,
	TAGMALLOC_CMDBUFF,
	TAGMALLOC_CMDTOKEN,
	TAGMALLOC_CMD,
	TAGMALLOC_LOADMAP,
	TAGMALLOC_ALIAS,
	TAGMALLOC_TRIGGER,
	TAGMALLOC_CVAR,
	TAGMALLOC_FSCACHE,
	TAGMALLOC_FSLOADFILE,
	TAGMALLOC_FSLOADPAK,
	TAGMALLOC_SEARCHPATH,
	TAGMALLOC_LINK,
	TAGMALLOC_CLIENTS,
	TAGMALLOC_CL_ENTS,
	TAGMALLOC_CL_BASELINES,
	TAGMALLOC_CL_MESSAGES,
	TAGMALLOC_CL_PARTICLES,

	TAGMALLOC_CLIENT_DOWNLOAD,
	TAGMALLOC_CLIENT_KEYBIND,
	TAGMALLOC_CLIENT_SFX,
	TAGMALLOC_CLIENT_SOUNDCACHE,
	TAGMALLOC_CLIENT_DLL,
	TAGMALLOC_CLIENT_LOC,
	TAGMALLOC_CLIENT_IGNORE,
	TAGMALLOC_BLACKHOLE,
	TAGMALLOC_CVARBANS,
	//TAGMALLOC_MSG_QUEUE,
	TAGMALLOC_CMDBANS,
	TAGMALLOC_REDBLACK,
	TAGMALLOC_LRCON,
#ifdef ANTICHEAT
	TAGMALLOC_ANTICHEAT,
#endif
	TAGMALLOC_MAX_TAGS
};


extern void (EXPORT *Z_Free)(const void *buf);
extern void *(EXPORT *Z_TagMalloc)(int size, int tag);

//void EXPORT Z_Free (void *ptr);
//void *Z_TagMalloc (int size, int tag);

void EXPORT Z_FreeGame (void *buf);
void RESTRICT * EXPORT Z_TagMallocGame (int size, int tag);
void EXPORT Z_FreeTagsGame (int tag);
void Z_Verify (const char *format, ...);
void Z_CheckGameLeaks (void);

void Qcommon_Init (int argc, char **argv);
void Qcommon_Frame (int msec);
void Qcommon_Shutdown (void);

#if defined _WIN32 && !defined _M_AMD64
size_t __cdecl fast_strlen(const char *s);
void __fastcall fast_strlwr(char *s);
int __cdecl fast_tolower(int c);
#else
#define fast_strlwr(x) Q_strlwr(x)
#define fast_strlen(x) strlen(x)
#define fast_tolower(x) tolower(x)
#endif
char *StripQuotes (char *string);

#ifdef _WIN32
//#ifdef _DEBUG
extern double totalPerformanceTime;
void _STOP_PERFORMANCE_TIMER (void);
void _START_PERFORMANCE_TIMER (void);
#define START_PERFORMANCE_TIMER _START_PERFORMANCE_TIMER()
#define STOP_PERFORMANCE_TIMER _STOP_PERFORMANCE_TIMER()
//#else
//#define START_PERFORMANCE_TIMER
//#define STOP_PERFORMANCE_TIMER
//#endif
#endif

#define NUMVERTEXNORMALS	162
extern	vec3_t	bytedirs[NUMVERTEXNORMALS];

// this is in the client code, but can be used for debugging from server
void EXPORT SCR_DebugGraph (float value, int color);

typedef enum {
	ss_dead,			// no map loaded
	ss_loading,			// spawning level edicts
	ss_game,			// actively running
	ss_cinematic,
	ss_demo,
	ss_pic
} server_state_t;

/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

void	Sys_Init (void);

void	Sys_AppActivate (void);
#ifndef NO_SERVER
void	Sys_UnloadGame (void);
void *Sys_GetGameAPI (void *parms, int baseq2DLL);
// loads the game dll and calls the api init function

char	*Sys_ConsoleInput (void);
void	Sys_ConsoleOutput (const char *string);
#endif
void	Sys_SendKeyEvents (void);
NORETURN void	Sys_Error (const char *error, ...) __attribute__ ((format (printf, 1, 2)));
void	Sys_Quit (void);
char	*Sys_GetClipboardData( void );
void	Sys_CopyProtect (void);
void	Sys_SetWindowText(char *buff);
void	Sys_ProcessTimes_f (void);
void	Sys_Spinstats_f (void);

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

void CL_Init (void);
void CL_Drop (qboolean skipdisconnect, qboolean nonerror);
void CL_Shutdown (void);
void CL_Frame (int msec);
void Con_Print (const char *text);
void SCR_BeginLoadingPlaque (void);

void SV_Init (void);
void SV_Shutdown (char *finalmsg, qboolean reconnect, qboolean crashing);
void SV_Frame (int msec);

#define _QCOMMON_H
#endif
