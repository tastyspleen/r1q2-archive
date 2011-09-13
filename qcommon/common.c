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
// common.c -- misc functions used in client and server
#include "qcommon.h"

#ifdef _WIN32
#define OPENSSLEXPORT __cdecl
#endif
#ifdef USE_OPENSSL
#include <openssl/md4.h>
#endif

#include <setjmp.h>

#define	MAXPRINTMSG	4096

#define MAX_NUM_ARGVS	50

qboolean		q2_initialized = false;

entity_state_t	null_entity_state;
usercmd_t		null_usercmd;
cvar_t			uninitialized_cvar;

cvar_t			*z_debug;
cvar_t			*z_buggygame;
cvar_t			*z_allowcorruption;

cvar_t			*cl_quietstartup;

static int		com_argc;
static char	*com_argv[MAX_NUM_ARGVS+1];

char	*binary_name;

//int		realtime;
jmp_buf abortframe;		// an ERR_DROP occured, exit the entire frame

//static FILE	*log_stats_file;

cvar_t	*host_speeds;
cvar_t	*log_stats;
cvar_t	*developer = &uninitialized_cvar;

static cvar_t	*timescale;
static cvar_t	*fixedtime;

static cvar_t	*logfile_active;	// 1 = buffer log, 2 = flush after each print
static cvar_t	*logfile_timestamp;
static cvar_t	*logfile_timestamp_format;
static cvar_t	*logfile_name;
static cvar_t	*logfile_filterlevel = &uninitialized_cvar;
static cvar_t	*con_filterlevel = &uninitialized_cvar;

#ifndef DEDICATED_ONLY
cvar_t	*showtrace;
#endif

#ifndef NO_SERVER
cvar_t	*dedicated = &uninitialized_cvar;
#endif

cvar_t	*sys_loopstyle = &uninitialized_cvar;

//r1: unload DLLs on crash?
cvar_t	*dbg_unload = &uninitialized_cvar;

//r1: throw int3 on ERR_FATAL?
static cvar_t	*dbg_crash_on_fatal_error = &uninitialized_cvar;

//r1: throw all err as fatal?
static cvar_t	*err_fatal = &uninitialized_cvar;

static FILE	*logfile;

int			server_state;

// host_speeds times
unsigned int		time_before_game;
unsigned int		time_after_game;
unsigned int		time_before_ref;
unsigned int		time_after_ref;

// for profiling
#ifndef NPROFILE
static int msg_local_hits;
static int msg_malloc_hits;

static int messageSizes[1500];
#endif

char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame",
	"svc_zpacket",
	"svc_zdownload",
	"svc_playerupdate",
	"svc_setting",
};

tagmalloc_tag_t tagmalloc_tags[] =
{
	{TAGMALLOC_NOT_TAGGED, "NOT_TAGGED", 0},
	{TAGMALLOC_CMDBUFF, "CMDBUFF", 0},
	{TAGMALLOC_CMDTOKEN, "CMDTOKEN", 0},
	{TAGMALLOC_CMD, "CMD", 0},
	{TAGMALLOC_LOADMAP, "LOADMAP", 0},
	{TAGMALLOC_ALIAS, "ALIAS", 0},
	{TAGMALLOC_TRIGGER, "TRIGGER", 0},
	{TAGMALLOC_CVAR, "CVAR", 0},
	{TAGMALLOC_FSCACHE, "FSCACHE", 0},
	{TAGMALLOC_FSLOADFILE, "FSLOADFILE", 0},
	{TAGMALLOC_FSLOADPAK, "FSLOADPAK", 0},
	{TAGMALLOC_SEARCHPATH, "SEARCHPATH", 0},
	{TAGMALLOC_LINK, "LINK", 0},
	{TAGMALLOC_CLIENTS, "CLIENTS", 0},
	{TAGMALLOC_CL_ENTS, "CL_ENTS", 0},
	{TAGMALLOC_CL_BASELINES, "CL_BASELINES", 0},
	{TAGMALLOC_CL_MESSAGES, "CL_MESSAGES", 0},
	{TAGMALLOC_CL_PARTICLES, "CL_PARTICLES", 0},

	{TAGMALLOC_CLIENT_DOWNLOAD, "CLIENT_DOWNLOAD", 0},
	{TAGMALLOC_CLIENT_KEYBIND, "CLIENT_KEYBIND", 0},
	{TAGMALLOC_CLIENT_SFX, "CLIENT_SFX", 0},
	{TAGMALLOC_CLIENT_SOUNDCACHE, "CLIENT_SOUNDCACHE", 0},
	{TAGMALLOC_CLIENT_DLL, "CLIENT_DLL", 0},
	{TAGMALLOC_CLIENT_LOC, "CLIENT_LOC", 0},
	{TAGMALLOC_CLIENT_IGNORE, "CLIENT_IGNORE", 0},
	{TAGMALLOC_BLACKHOLE, "BLACKHOLE", 0},
	{TAGMALLOC_CVARBANS, "CVARBANS", 0},
	//{TAGMALLOC_MSG_QUEUE, "MSGQUEUE", 0},
	{TAGMALLOC_CMDBANS, "CMDBANS", 0},
	{TAGMALLOC_REDBLACK, "REDBLACK", 0},
	{TAGMALLOC_LRCON, "LRCON", 0},
#ifdef ANTICHEAT
	{TAGMALLOC_ANTICHEAT, "ANTICHEAT", 0},
#endif
	{TAGMALLOC_MAX_TAGS, "*** UNDEFINED ***", 0}
};

void (EXPORT *Z_Free)(const void *buf);
void *(EXPORT *Z_TagMalloc)(int size, int tag);

/*
============================================================================

CLIENT / SERVER interactions

============================================================================
*/

int	rd_target;

static char	*rd_buffer;
static int	rd_buffersize;
static void	(*rd_flush)(int target, char *buffer);

void Com_BeginRedirect (int target, char *buffer, int buffersize, void *flush)
{
	if (!target || !buffer || !buffersize || !flush)
		return;
	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = (void (*)(int, char *))flush;

	*rd_buffer = 0;
}

void Com_EndRedirect (qboolean flush)
{
	if (!rd_target)
		return;

	if (flush)
		rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/*
=============
Com_Printf

Both client and server can use this, and it will output
to the apropriate place.
=============
*/
void Com_Printf (const char *fmt, int level, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ((level & con_filterlevel->intvalue) && (level & logfile_filterlevel->intvalue))
		return;

	va_start (argptr, level);
	if (Q_vsnprintf (msg, sizeof(msg)-1, fmt, argptr) < 0)
		Com_Printf ("WARNING: Com_Printf: message overflow.\n", LOG_GENERAL);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	if (rd_target)
	{
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1))
		{
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}
		strcat (rd_buffer, msg);
		return;
	}

	if (!(level & con_filterlevel->intvalue))
	{
#ifndef DEDICATED_ONLY
		Con_Print (msg);
#endif

	// also echo to debugging console
#ifndef NO_SERVER
		Sys_ConsoleOutput (msg);
#endif
	}

	// logfile
	if (logfile_active && logfile_active->intvalue && !(level & logfile_filterlevel->intvalue))
	{
		char	name[MAX_QPATH];
		char	timestamp[64];
		char	*p;

		//r1: strip highbits and control chars
		p = msg;
		while (p[0])
		{
			p[0] &= ~128;
			if (p[0] < 32 && !isspace(p[0]))
				p[0] = '-';
			p++;
		}

		if (!logfile)
		{
			//ensure someone malicious with rcon can't overwrite arbitrary files...
			if (strstr (logfile_name->string, "..") || strchr (logfile_name->string, '/') || strchr (logfile_name->string, '\\'))
			{
				Cvar_ForceSet ("logfile", "0");
				Com_Printf ("ALERT: Refusing to open logfile %s, illegal filename.\n", LOG_GENERAL|LOG_WARNING, logfile_name->string);
				return;
			}
			Com_sprintf (name, sizeof(name), "%s/%s", FS_Gamedir (), logfile_name->string);

			if (logfile_active->intvalue > 2)
				logfile = fopen (name, "a");
			else
				logfile = fopen (name, "w");

			if (!logfile)
			{
				Cvar_ForceSet ("logfile", "0");
				Com_Printf ("ALERT: Couldn't open logfile %s for writing, logfile disabled.\n", LOG_GENERAL|LOG_WARNING, logfile_name->string);
				return;
			}
		}

		if (logfile)
		{
			if (logfile_timestamp->intvalue)
			{
				char		*line;
				char		*msgptr;

				static qboolean	insert_timestamp = true;

				time_t	tm;
				time(&tm);
				strftime (timestamp, sizeof(timestamp)-1, logfile_timestamp_format->string, localtime(&tm));

				msgptr = msg;
				line = strchr (msgptr, '\n');
				while (line)
				{
					if (insert_timestamp)
					{
						fprintf (logfile, "%s ", timestamp);
						insert_timestamp = false;
					}
					*line = 0;

					if (fprintf (logfile, "%s\n", msgptr) < 0)
					{
						fclose (logfile);
						logfile = NULL;
						Cvar_ForceSet ("logfile", "0");
						Com_Printf ("ALERT: Error writing to logfile %s, file closed.\n", LOG_GENERAL|LOG_WARNING, logfile_name->string);
						return;
					}
						
					insert_timestamp = true;
					line++;
					msgptr = line;

					if (!*line)
						break;

					line = strchr (msgptr, '\n');
				}

				if (insert_timestamp)
				{
					fprintf (logfile, "%s ", timestamp);
					insert_timestamp = false;
				}

				fprintf (logfile, "%s", msgptr);
			}
			else
			{
				fwrite (msg, strlen(msg), 1, logfile);
			}
		}

		//r1: allow logging > 2 (append) but not forcing flushing.
		if (logfile_active->intvalue & 1)
			fflush (logfile);
	}
}


/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void _Com_DPrintf (char const *fmt, ...)
{
#if !(__STDC_VERSION__ == 199901L || _MSC_VER >= 1400 && !defined _M_AMD64)
	if (!developer->intvalue)
	{
		return;
	}
	else
#endif
	{
		va_list		argptr;
		char		msg[MAXPRINTMSG];

		va_start (argptr,fmt);
		if (Q_vsnprintf (msg, sizeof(msg)-1, fmt, argptr) < 0)
			Com_Printf ("WARNING: Com_DPrintf: message overflow.\n", LOG_WARNING);
		va_end (argptr);

		msg[sizeof(msg)-1] = 0;
	
		Com_Printf ("%s", LOG_DEBUG, msg);
	}
}


/*
=============
Com_Error

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Error (int code, const char *fmt, ...)
{
	va_list		argptr;
	static char		msg[MAXPRINTMSG];
	static	qboolean	recursive;

	if (recursive && code != ERR_DIE)
		Sys_Error ("recursive error after: %s", msg);
	recursive = true;

	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	Com_EndRedirect (false);

	if (err_fatal->intvalue)
		code = ERR_FATAL;
	
	if (code == ERR_DISCONNECT)
	{
		Com_Printf ("Disconnected by server!\n", LOG_CLIENT);
#ifndef DEDICATED_ONLY
		CL_Drop (false, true);
#endif
		recursive = false;

		longjmp (abortframe, -1);
	}
	else if (code == ERR_DROP || code == ERR_GAME || code == ERR_NET || code == ERR_HARD)
	{
		int	state = Com_ServerState() == ss_dead ? 0 : 1;
		Com_Printf ("********************\nERROR: %s\n********************\n", LOG_GENERAL, msg);
#ifndef NO_SERVER
		SV_Shutdown (va("Server exited: %s\n", msg), false, false);
#endif
#ifndef DEDICATED_ONLY
		CL_Drop (code == ERR_NET, false);
#endif
		recursive = false;

		//r1: auto-restart server code on game crash
		if (state && (code == ERR_GAME || code == ERR_DROP))
		{
			const char *resmap;

			resmap = Cvar_VariableString ("sv_restartmap");

			if (resmap[0])
				Cmd_ExecuteString (va ("map %s", resmap));
		}

		longjmp (abortframe, -1);
	}
	else
	{
		printf ("%s\n", msg);
		if (dbg_crash_on_fatal_error->intvalue)
			Sys_DebugBreak ();

		//an err_die means the whole game is about to explode, avoid running any extra code if possible
		if (code != ERR_DIE)
		{
#ifndef NO_SERVER
			SV_Shutdown (va("Server fatal crashed: %s\n", msg), false, true);
#endif
#ifndef DEDICATED_ONLY
			if (dbg_unload->intvalue)
				CL_Shutdown ();
#endif
		}
	}

	if (logfile)
	{
		fprintf (logfile, "Fatal Error\n*****************************\n"
						  "Server fatal crashed: %s\n"
						  "*****************************\n", msg);
		fclose (logfile);
		logfile = NULL;
	}

	Sys_Error ("%s", msg);
	recursive = false;
}


/*
=============
Com_Quit

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Quit (void)
{
	//r1: optional quit message, reworded "Server quit"
#ifndef NO_SERVER
	if (Cmd_Argc() > 1)
		SV_Shutdown (va("Server has shut down: %s\n", Cmd_Args()), false, false);
	else
		SV_Shutdown ("Server has shut down\n", false, false);
#endif
#ifndef DEDICATED_ONLY
	CL_Shutdown ();
#endif

	if (logfile)
	{
		fclose (logfile);
		logfile = NULL;
	}

	Sys_Quit ();
}


/*
==================
Com_ServerState
==================
*/
/*int Com_ServerState (void)
{
	return server_state;
}*/

/*
==================
Com_SetServerState
==================
*/
/*void Com_SetServerState (int state)
{
	server_state = state;
}*/


/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

vec3_t	bytedirs[NUMVERTEXNORMALS] =
{
#include "../client/anorms.h"
};

//
// writing functions
//

byte		message_buff[0x10000];
sizebuf_t	msgbuff;

void MSG_WriteChar (int c)
{
	byte	*buf;
	
/*#ifdef PARANOID
	if (c < -128 || c > 127)
		Com_Error (ERR_FATAL, "MSG_WriteChar: range error");
#endif*/
	Q_assert (!(c < -128 || c > 127));

	buf = SZ_GetSpace (&msgbuff, 1);
	buf[0] = c;
}

void MSG_BeginWriting (int c)
{
	byte	*buf;

/*#ifdef PARANOID
	if (c < 0 || c > 255)
		Com_Error (ERR_FATAL, "MSG_WriteByte: range error");
#endif*/
	Q_assert (!(c < 0 || c > 255));
	Q_assert (!msgbuff.cursize);

#ifdef _DEBUG
	memset (message_buff, 0xcc, sizeof(message_buff));
#endif

	buf = SZ_GetSpace (&msgbuff, 1);
	buf[0] = c;
}

void MSG_WriteByte (int c)
{
	byte	*buf;

/*#ifdef PARANOID
	if (c < 0 || c > 255)
		Com_Error (ERR_FATAL, "MSG_WriteByte: range error");
#endif*/
	Q_assert (!(c < 0 || c > 255));

	buf = SZ_GetSpace (&msgbuff, 1);
	buf[0] = c;
}

void MSG_WriteShort (int c)
{
	byte	*buf;
	
/*#ifdef PARANOID
	if (c < ((int16)0x8000) || c > (int16)0x7fff)
		Com_Error (ERR_FATAL, "MSG_WriteShort: range error");
#endif*/
	//XXX: unsigned shorts are written here too...
	//Q_assert (!(c < ((int16)0x8000) || c > (int16)0x7fff));

	buf = SZ_GetSpace (&msgbuff, 2);
	buf[0] = c&0xff;
	buf[1] = (c>>8) &0xff;
}

void SZ_WriteShort (sizebuf_t *sbuf, int c)
{
	byte	*buf;

	buf = SZ_GetSpace (sbuf, 2);
	buf[0] = c&0xff;
	buf[1] = (c>>8) &0xff;
}

void SZ_WriteLong (sizebuf_t *sbuf, int c)
{
	byte	*buf;
	
	buf = SZ_GetSpace (sbuf, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void SZ_WriteByte (sizebuf_t *sbuf, int c)
{
	byte	*buf;

	Q_assert (!(c < 0 || c > 255));

	buf = SZ_GetSpace (sbuf, 1);
	buf[0] = c;
}

void MSG_WriteLong (int c)
{
	byte	*buf;
	
	buf = SZ_GetSpace (&msgbuff, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (float f)
{
	union
	{
		float	f;
		int	l;
	} dat;
	
	
	dat.f = f;
	dat.l = LittleLong (dat.l);
	
	SZ_Write (&msgbuff, &dat.l, 4);
}

void MSG_WriteString (const char *s)
{
	if (!s)
		SZ_Write (&msgbuff, "", 1);
	else
		SZ_Write (&msgbuff, s, (int)strlen(s)+1);
}

void MSG_Write (const void *data, int length)
{
	memcpy (SZ_GetSpace(&msgbuff,length),data,length);		
}

void MSG_Print (const char *data)
{
	int		len;
	
	len = (int)strlen(data)+1;

	Q_assert (len > 1);

	if (msgbuff.cursize)
	{
		if (message_buff[msgbuff.cursize-1])
			memcpy ((byte *)SZ_GetSpace(&msgbuff, len),data,len); // no trailing 0
		else
			memcpy ((byte *)SZ_GetSpace(&msgbuff, len-1)-1,data,len); // write over trailing 0
	}
	else
		memcpy ((byte *)SZ_GetSpace(&msgbuff, len),data,len);
}

int	MSG_GetLength (void)
{
	return msgbuff.cursize;
}

sizebuf_t *MSG_GetRawMsg (void)
{
	return &msgbuff;
}

byte *MSG_GetData (void)
{
	return message_buff;
}

byte MSG_GetType (void)
{
	Q_assert (msgbuff.cursize > 0);

	return message_buff[0];
}

void MSG_FreeData (void)
{
	Q_assert (msgbuff.cursize > 0);
	SZ_Clear (&msgbuff);
#ifdef _DEBUG
	memset (message_buff, 0xcc, sizeof(message_buff));
#endif
}

void MSG_Clear (void)
{
	SZ_Clear (&msgbuff);
#ifdef _DEBUG
	memset (message_buff, 0xcc, sizeof(message_buff));
#endif
}

void MSG_EndWriting (sizebuf_t *out)
{
	Q_assert (msgbuff.cursize > 0);

	//Q_assert (msgbuff.cursize < MAX_USABLEMSG);

	if (out->cursize + msgbuff.cursize > out->maxsize)
	{
		Com_DPrintf ("MSG_EndWriting: overflow\n");
		SZ_Clear (out);
		out->overflowed = true;
	}
	else
	{
		SZ_Write (out, message_buff, msgbuff.cursize);
	}

	SZ_Clear (&msgbuff);
#ifdef _DEBUG
	memset (message_buff, 0xcc, sizeof(message_buff));
#endif
}

void MSG_EndWrite (messagelist_t *out)
{
	Q_assert (msgbuff.cursize > 0);

	//r1: use small local buffer if possible to avoid thousands of mallocs with tiny amounts
	if (msgbuff.cursize > MSG_MAX_SIZE_BEFORE_MALLOC)
	{
#ifndef NPROFILE
		msg_malloc_hits++;
#endif
		out->data = malloc (msgbuff.cursize);
	}
	else
	{
#ifndef NPROFILE
		msg_local_hits++;
#endif
		out->data = out->localbuff;
	}

#ifndef NPROFILE
	if (msgbuff.cursize < sizeof(messageSizes) / sizeof(messageSizes[0]))
		messageSizes[msgbuff.cursize]++;
#endif

	memcpy (out->data, message_buff, msgbuff.cursize);
	out->cursize = msgbuff.cursize;
}

void MSG_WriteCoord (float f)
{
	MSG_WriteShort ((int)(f*8));
}

void MSG_WritePos (vec3_t pos)
{
	MSG_WriteShort ((int)(pos[0]*8));
	MSG_WriteShort ((int)(pos[1]*8));
	MSG_WriteShort ((int)(pos[2]*8));
}

void MSG_WriteAngle (float f)
{
	MSG_WriteByte ((int)(f*256/360) & 255);
}

void MSG_WriteAngle16 (float f)
{
	MSG_WriteShort (ANGLE2SHORT(f));
}


void MSG_WriteDeltaUsercmd (const usercmd_t *from, const usercmd_t *cmd, int protocol)
{
	int		bits;
	int		buttons;

//
// send the movement message
//
	bits = 0;
	buttons = 0;

	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;

	//FIXME: it's impossible for cl.viewangles roll to change,
	//is this ever legitimately set?
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;

	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;

	if (cmd->buttons != from->buttons)
	{
		buttons = cmd->buttons;
		bits |= CM_BUTTONS;
	}

	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

    MSG_WriteByte (bits);

	//waste not what precious bytes we have...
	if (protocol >= MINOR_VERSION_R1Q2_UCMD_UPDATES)
	{
		if (bits & CM_BUTTONS)
		{
			if ((bits & CM_FORWARD) && (cmd->forwardmove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLFORWARD;
			if ((bits & CM_SIDE) && (cmd->sidemove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLSIDE;
			if ((bits & CM_UP) && (cmd->upmove % 5) == 0)
				buttons |= BUTTON_UCMD_DBLUP;

			if ((bits & CM_ANGLE1) && (cmd->angles[0] % 64) == 0 && (abs(cmd->angles[0] / 64)) < 128)
				buttons |= BUTTON_UCMD_DBL_ANGLE1;
			if ((bits & CM_ANGLE2) && (cmd->angles[1] % 256) == 0)
				buttons |= BUTTON_UCMD_DBL_ANGLE2;

			MSG_WriteByte (buttons);
		}
	}

	if (bits & CM_ANGLE1)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE1)
			MSG_WriteChar (cmd->angles[0] / 64);
		else
			MSG_WriteShort (cmd->angles[0]);
	}

	if (bits & CM_ANGLE2)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE2)
			MSG_WriteChar (cmd->angles[1] / 256);
		else
			MSG_WriteShort (cmd->angles[1]);
	}

	if (bits & CM_ANGLE3)
		MSG_WriteShort (cmd->angles[2]);
	
	if (bits & CM_FORWARD)
	{
		if (buttons & BUTTON_UCMD_DBLFORWARD)
			MSG_WriteChar (cmd->forwardmove / 5);
		else
			MSG_WriteShort (cmd->forwardmove);
	}

	if (bits & CM_SIDE)
	{
		if (buttons & BUTTON_UCMD_DBLSIDE)
			MSG_WriteChar (cmd->sidemove / 5);
		else
			MSG_WriteShort (cmd->sidemove);
	}

	if (bits & CM_UP)
	{
		if (buttons & BUTTON_UCMD_DBLUP)
			MSG_WriteChar (cmd->upmove / 5);
		else
			MSG_WriteShort (cmd->upmove);
	}

	if (protocol < MINOR_VERSION_R1Q2_UCMD_UPDATES)
	{
 		if (bits & CM_BUTTONS)
	  		MSG_WriteByte (buttons);
	}

 	if (bits & CM_IMPULSE)
	    MSG_WriteByte (cmd->impulse);

    MSG_WriteByte (cmd->msec);
	MSG_WriteByte (cmd->lightlevel);
}


void MSG_WriteDir (vec3_t dir)
{
	int		i, best;
	float	d, bestd;
	
	if (!dir)
	{
		MSG_WriteByte (0);
		return;
	}

	bestd = 0;
	best = 0;
	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct (dir, bytedirs[i]);
		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}
	MSG_WriteByte (best);
}


void MSG_ReadDir (sizebuf_t *sb, vec3_t dir)
{
	int		b;

	b = MSG_ReadByte (sb);
	if (b == -1 || b >= NUMVERTEXNORMALS)
		Com_Error (ERR_DROP, "MSG_ReadDir: out of range (%d)", b);
	VectorCopy (bytedirs[b], dir);
}


//============================================================

//
// reading functions
//

void MSG_BeginReading (sizebuf_t *msg)
{
	msg->readcount = 0;
}

// returns -1 if no more characters are available
int MSG_ReadChar (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+1 > msg_read->cursize)
		c = -1;
	else
		c = (signed char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;
	
	return c;
}

int MSG_ReadByte (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+1 > msg_read->cursize)
		c = -1;
	else
		c = (unsigned char)msg_read->data[msg_read->readcount];
	msg_read->readcount++;
	
	return c;
}

int MSG_ReadShort (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+2 > msg_read->cursize)
		c = -1;
	else		
		c = (int16)(msg_read->data[msg_read->readcount]
		+ (msg_read->data[msg_read->readcount+1]<<8));
	
	msg_read->readcount += 2;
	
	return c;
}

int MSG_ReadLong (sizebuf_t *msg_read)
{
	int	c;
	
	if (msg_read->readcount+4 > msg_read->cursize)
		c = -1;
	else
		c = msg_read->data[msg_read->readcount]
		+ (msg_read->data[msg_read->readcount+1]<<8)
		+ (msg_read->data[msg_read->readcount+2]<<16)
		+ (msg_read->data[msg_read->readcount+3]<<24);
	
	msg_read->readcount += 4;
	
	return c;
}

float MSG_ReadFloat (sizebuf_t *msg_read)
{
	union
	{
		byte	b[4];
		float	f;
		int	l;
	} dat;
	
	if (msg_read->readcount+4 > msg_read->cursize)
		dat.f = -1;
	else
	{
		dat.b[0] =	msg_read->data[msg_read->readcount];
		dat.b[1] =	msg_read->data[msg_read->readcount+1];
		dat.b[2] =	msg_read->data[msg_read->readcount+2];
		dat.b[3] =	msg_read->data[msg_read->readcount+3];
	}
	msg_read->readcount += 4;
	
	dat.l = LittleLong (dat.l);

	return dat.f;	
}

char *MSG_ReadString (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l,c;
	
	l = 0;
	do
	{
		c = MSG_ReadByte (msg_read);
		if (c == -1 || c == 0)
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	
	return string;
}

char *MSG_ReadStringLine (sizebuf_t *msg_read)
{
	static char	string[2048];
	int		l,c;
	
	l = 0;
	do
	{
		c = MSG_ReadByte (msg_read);
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = 0;
	return string;
}

float MSG_ReadCoord (sizebuf_t *msg_read)
{
	return MSG_ReadShort(msg_read) * 0.125f;
}

void MSG_ReadPos (sizebuf_t *msg_read, vec3_t pos)
{
	pos[0] = MSG_ReadShort(msg_read) * 0.125f;
	pos[1] = MSG_ReadShort(msg_read) * 0.125f;
	pos[2] = MSG_ReadShort(msg_read) * 0.125f;
}

float MSG_ReadAngle (sizebuf_t *msg_read)
{
	return MSG_ReadChar(msg_read) * 1.40625f;
}

float MSG_ReadAngle16 (sizebuf_t *msg_read)
{
	return SHORT2ANGLE(MSG_ReadShort(msg_read));
}

#ifndef NPROFILE
unsigned long r1q2UserCmdOptimizedBytes = 0;
#endif

void MSG_ReadDeltaUsercmd (sizebuf_t *msg_read, usercmd_t *from, usercmd_t /*@out@*/*move, int protocol)
{
	int			buttons;
	int			bits;
	unsigned	msec;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte (msg_read);
	buttons = 0;
		
	if (protocol >= MINOR_VERSION_R1Q2_UCMD_UPDATES)
	{
		if (bits & CM_BUTTONS)
		{
			buttons = MSG_ReadByte (msg_read);

			//only save real buttons, strip off the extra stolen bits
			move->buttons = buttons & (BUTTON_ATTACK|BUTTON_USE|BUTTON_ANY);
		}
	}

// read current angles
	if (bits & CM_ANGLE1)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE1)
		{
			#ifndef NPROFILE
			r1q2UserCmdOptimizedBytes++;
			#endif
			move->angles[0] = MSG_ReadChar (msg_read) * 64;
		}
		else
			move->angles[0] = MSG_ReadShort (msg_read);
	}

	if (bits & CM_ANGLE2)
	{
		if (buttons & BUTTON_UCMD_DBL_ANGLE2)
		{
			#ifndef NPROFILE
			r1q2UserCmdOptimizedBytes++;
			#endif
			move->angles[1] = MSG_ReadChar (msg_read) * 256;
		}
		else
			move->angles[1] = MSG_ReadShort (msg_read);
	}

	if (bits & CM_ANGLE3)
		move->angles[2] = MSG_ReadShort (msg_read);
		
// read movement
	if (bits & CM_FORWARD)
	{
		if (buttons & BUTTON_UCMD_DBLFORWARD)
		{
			#ifndef NPROFILE
			r1q2UserCmdOptimizedBytes++;
			#endif
			move->forwardmove = MSG_ReadChar (msg_read) * 5;
		}
		else
			move->forwardmove = MSG_ReadShort (msg_read);
	}

	if (bits & CM_SIDE)
	{
		if (buttons & BUTTON_UCMD_DBLSIDE)
		{
			#ifndef NPROFILE
			r1q2UserCmdOptimizedBytes++;
			#endif
			move->sidemove = MSG_ReadChar (msg_read) * 5;
		}
		else
			move->sidemove = MSG_ReadShort (msg_read);
	}

	if (bits & CM_UP)
	{
		if (buttons & BUTTON_UCMD_DBLUP)
		{
			#ifndef NPROFILE
			r1q2UserCmdOptimizedBytes++;
			#endif
			move->upmove = MSG_ReadChar (msg_read) * 5;
		}
		else
			move->upmove = MSG_ReadShort (msg_read);
	}
	
// read buttons
	if (protocol < MINOR_VERSION_R1Q2_UCMD_UPDATES)
	{
		if (bits & CM_BUTTONS)
			move->buttons = MSG_ReadByte (msg_read);
	}

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte (msg_read);

// read time to run command
	msec = MSG_ReadByte (msg_read);

	if (msec > 250)
		Com_Printf ("MSG_ReadDeltaUsercmd: funky msec (%d)!\n", LOG_GENERAL, msec);

	move->msec = msec;

// read the light level
	move->lightlevel = MSG_ReadByte (msg_read);
}


void MSG_ReadData (sizebuf_t *msg_read, void *data, int len)
{
	int		i;

	for (i=0 ; i<len ; i++)
		((byte *)data)[i] = MSG_ReadByte (msg_read);
}


//===========================================================================

void SZ_Init (sizebuf_t /*@out@*/*buf, byte /*@out@*/*data, int length)
{
	Q_assert (length > 0);

	memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->buffsize = length;	//should never change this
}

void SZ_Clear (sizebuf_t /*@out@*/*buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace (sizebuf_t /*@out@*/*buf, int length)
{
	void	*data;

	Q_assert (length > 0);
	
	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->data)
			Com_Error (ERR_FATAL, "SZ_GetSpace: attempted to write %d bytes to an uninitialized buffer!", length);

		if (!buf->allowoverflow)
		{
			if (length > buf->maxsize)
				Com_Error (ERR_FATAL, "SZ_GetSpace: %i is > full buffer size %d (%d)", length, buf->maxsize, buf->buffsize);

			Com_Error (ERR_FATAL, "SZ_GetSpace: overflow without allowoverflow set (%d+%d > %d)", buf->cursize, length, buf->maxsize);
		}		
		
		//r1: clear the buffer BEFORE the error!! (for console buffer)
		if (buf->cursize + length >= buf->buffsize)
		{
			SZ_Clear (buf);
			Com_DPrintf ("SZ_GetSpace: overflow\n");
		}
		else
		{
			Com_DPrintf ("SZ_GetSpace: overflowed maxsize\n");
		}

		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;
	
	return data;
}

void SZ_Write (sizebuf_t /*@out@*/*buf, const void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);		
}

void SZ_Print (sizebuf_t /*@out@*/*buf, const char *data)
{
	int		len;
	
	len = (int)strlen(data)+1;

	Q_assert (len > 1);

	if (buf->cursize)
	{
		if (buf->data[buf->cursize-1])
			memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
		else
			memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
	}
	else
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len);
}


//============================================================================


/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
/*int COM_CheckParm (const char *parm)
{
	int		i;
	
	for (i=1 ; i<com_argc ; i++)
	{
		if (!strcmp (parm,com_argv[i]))
			return i;
	}
		
	return 0;
}*/

int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

void COM_ClearArgv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}


/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	int		i;

	if (argc > MAX_NUM_ARGVS)
		Com_Error (ERR_FATAL, "argc > MAX_NUM_ARGVS");
	com_argc = argc;
	for (i=0 ; i<argc ; i++)
	{
		if (!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS )
			com_argv[i] = "";
		else
			com_argv[i] = argv[i];
	}
}

/*
================
COM_AddParm

Adds the given string at the end of the current argument list
================
*/
void COM_AddParm (char *parm)
{
	if (com_argc == MAX_NUM_ARGVS)
		Com_Error (ERR_FATAL, "COM_AddParm: MAX_NUM_ARGVS");
	com_argv[com_argc++] = parm;
}




/// just for debugging
/*int	memsearch (byte *start, int count, int search)
{
	int		i;
	
	for (i=0 ; i<count ; i++)
		if (start[i] == search)
			return i;
	return -1;
}*/


char *CopyString (const char *in, int tag)
{
	char	*out;
	
	out = Z_TagMalloc ((int)strlen(in)+1, tag);
	strcpy (out, in);
	return out;
}

void Info_Print (const char *s)
{
	char	key[512];
	char	value[512];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = (int)(o - key);
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s", LOG_GENERAL, key);

		if (!*s)
		{
			Com_Printf ("Info_Print: MISSING VALUE\n", LOG_GENERAL);
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", LOG_GENERAL, value);
	}
}

/*
==============================================================================

						ZONE MEMORY ALLOCATION

just cleared malloc with counters now...

==============================================================================
*/

#define	Z_MAGIC			0x1d1d
#define	Z_MAGIC_DEBUG	0x2d2d

typedef struct zhead_s
{
	struct zhead_s	*prev, *next;
	int16	magic;
	int16	tag;			// for group free
	int		size;
	void	*allocationLocation;
} zhead_t;

typedef struct z_memloc_s
{
	void				*address;
	uint32				time;
	uint32				size;
	struct	z_memloc_s	*next;
	void				*allocationLocation;
} z_memloc_t;

static z_memloc_t	z_game_locations;

static zhead_t	z_chain = {0};

static long		z_count = 0;
static long		z_bytes = 0;

static unsigned long	z_allocs = 0;
static unsigned long	z_level_allocs = 0;
static unsigned long	z_game_allocs = 0;

qboolean	free_from_game = false;

/*
========================
Z_Free
========================
*/
void EXPORT Z_FreeRelease (const void *ptr)
{
	zhead_t	*z;

	z = ((zhead_t *)ptr) - 1;

	if (z->magic != Z_MAGIC && z->magic != Z_MAGIC_DEBUG)
		Com_Error (ERR_DIE, "Z_Free: bad magic");

	z->prev->next = z->next;
	z->next->prev = z->prev;

	z_count--;
	z_bytes -= z->size;
	free (z);
}

void EXPORT Z_FreeDebug (const void *ptr)
{
	zhead_t	*z;

	if (!ptr)
		Com_Error (ERR_DIE, "Z_Free: null pointer given from %s", free_from_game ? "GAME" : "EXECUTABLE");

	z = ((zhead_t *)ptr) - 1;

	Z_Verify ("Z_FreeDebug: START FREE FROM %s OF %p (%d bytes tagged %d (%s))", free_from_game ? "GAME" : "EXECUTABLE", ptr, z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG");

	//magic test
	if (z->magic != Z_MAGIC && z->magic != Z_MAGIC_DEBUG)
		Com_Error (ERR_DIE, "Z_Free: bad magic freeing %p from %s", z, free_from_game ? "GAME" : "EXECUTABLE");

	//size sanity test
	if (z->size <= 0 || z->size > 0x40000000)
	{
		if (z_allowcorruption->intvalue)
			Com_Printf ("Z_Free: crazy block size %d (maybe tag %d (%s)) from %s at %p, allocated at %p", LOG_ERROR, z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", free_from_game ? "GAME" : "EXECUTABLE", z, z->allocationLocation);
		else
			Com_Error (ERR_DIE, "Z_Free: crazy block size %d (maybe tag %d (%s)) from %s at %p allocated at %p", z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", free_from_game ? "GAME" : "EXECUTABLE", z, z->allocationLocation);
	}

	//we could segfault here if size is invalid :(
	if (z->magic == Z_MAGIC_DEBUG && (*(byte **)&z)[z->size-1] != 0xCC)
	{
		if (z_allowcorruption->intvalue)
			Com_Printf ("Z_Free: buffer overrun detected in block sized %d (tagged as %d (%s)) from %s at %p allocated at %p", LOG_ERROR, z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", free_from_game ? "GAME" : "EXECUTABLE", z, z->allocationLocation);
		else
			Com_Error (ERR_DIE, "Z_Free: buffer overrun detected in block sized %d (tagged as %d (%s)) from %s at %p allocated at %p", z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", free_from_game ? "GAME" : "EXECUTABLE", z, z->allocationLocation);
	}

	z->prev->next = z->next;
	z->next->prev = z->prev;

	if (z->next->magic != Z_MAGIC && z->next->magic != Z_MAGIC_DEBUG)
		Com_Error (ERR_DIE, "Z_Free: memory corruption detected after free of block at %p from %s", z, free_from_game ? "GAME" : "EXECUTABLE");

	z_count--;
	z_bytes -= z->size;

	if (z_count < 0 || z_bytes < 0)
		Com_Error (ERR_DIE, "Z_Free: counters are screwed after free of %d bytes at %p tagged %d (%s) from %s", z->size, z, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", free_from_game ? "GAME" : "EXECUTABLE");

	free (z);

	Z_Verify ("Z_FreeDebug: END FREE OF %p FROM %s", ptr, free_from_game ? "GAME" : "EXECUTABLE");
}

/*
========================
Z_Stats_f
========================
*/

#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level

void Z_Stats_f (void)
{
	int i, total, num, bigtotal, bignum, level_count, level_size, game_count, game_size;
	zhead_t	*z, *next;

	bigtotal = bignum = level_size = level_count = game_size = game_count = 0;

	for (i = 0; i < TAGMALLOC_MAX_TAGS; i++) {
		total = num = 0;
		for (z=z_chain.next ; z != &z_chain ; z=next)
		{
			next = z->next;
			if (z->tag == i) {
				total += z->size;
				num++;
			}

			if (i == (TAGMALLOC_MAX_TAGS - 1)) {
				if (z->tag == TAG_LEVEL) {
					level_size += z->size;
					level_count ++;
				} else if (z->tag == TAG_GAME) {
					game_size += z->size;
					game_count++;
				}
			}
		}
		bigtotal += total;
		bignum += num;
		Com_Printf ("%14.14s: %8i bytes %5i blocks %8i allocs\n", LOG_GENERAL, tagmalloc_tags[i].name, total, num, tagmalloc_tags[i].allocs);
	}

	bigtotal += game_size;
	bigtotal += level_size;

	bignum += game_count;
	bignum += level_count;

	Com_Printf ("%14.14s: %8i bytes %5i blocks %8lu allocs\n", LOG_GENERAL, "DLL_LEVEL", level_size, level_count, z_level_allocs);
	Com_Printf ("%14.14s: %8i bytes %5i blocks %8lu allocs\n\n", LOG_GENERAL, "DLL_GAME", game_size, game_count, z_game_allocs);
	
	Com_Printf ("%lu unaccounted allocations\n", LOG_GENERAL, z_allocs);

	Com_Printf ("  CALCED_TOTAL: %i bytes in %i blocks\n", LOG_GENERAL, bigtotal, bignum);
	Com_Printf (" RUNNING_TOTAL: %li bytes in %li blocks\n", LOG_GENERAL, z_bytes, z_count);
}

/*
========================
Z_FreeTags
========================
*/
void Z_FreeTags (int tag)
{
	zhead_t	*z, *next;

	Z_Verify ("Z_FreeTags: START");

	for (z=z_chain.next ; z != &z_chain ; z=next)
	{
		next = z->next;
		if (z->tag == tag)
			Z_Free ((void *)(z+1));
	}

	Z_Verify ("Z_FreeTags: END");
}

/*
========================
Z_FreeTags
========================
*/
void Z_Verify (const char *format, ...)
{
	va_list		argptr;
	int			i;
	zhead_t		*z, *next;
	char		string[1024];
	
	va_start (argptr, format);
	vsnprintf (string, sizeof(string)-1, format, argptr);
	va_end (argptr);

	string[sizeof(string)-1] = 0;

	i = 0;

	for (z=z_chain.next ; z != &z_chain ; z=next)
	{
		next = z->next;
		if (z->magic != Z_MAGIC)
		{
			if (z->magic == Z_MAGIC_DEBUG)
			{
				//size sanity test
				if (z->size <= 0 || z->size > 0x40000000)
				{
					if (z_allowcorruption->intvalue)
						Com_Printf ("Z_Verify: crazy block size %d (maybe tag %d (%s)) during '%s' at %p, allocated from %p", LOG_ERROR, z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", string, z, z->allocationLocation);
					else
						Com_Error (ERR_DIE, "Z_Verify: crazy block size %d (maybe tag %d (%s)) during '%s' at %p, allocated from %p", z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", string, z, z->allocationLocation);
				}

				//we could segfault here if size is invalid :(
				if ((*(byte **)&z)[z->size-1] != 0xCC)
				{
					if (z_allowcorruption->intvalue)
						Com_Printf ("Z_Verify: buffer overrun detected in block sized %d (tagged as %d (%s)) during '%s' at %p, allocated from %p", LOG_ERROR, z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", string, z, z->allocationLocation);
					else
						Com_Error (ERR_DIE, "Z_Verify: buffer overrun detected in block sized %d (tagged as %d (%s)) during '%s' at %p, allocated from %p", z->size, z->tag, z->tag < TAGMALLOC_MAX_TAGS ? tagmalloc_tags[z->tag].name : "UNKNOWN TAG", string, z, z->allocationLocation);
				}
			}
			else
			{
				if (z_allowcorruption->intvalue)
					Com_Printf ("Z_Verify: memory corruption detected during '%s' in block %p allocated from %p", LOG_ERROR, string, (void *)(z+1), z->allocationLocation);
				else
					Com_Error (ERR_DIE, "Z_Verify: memory corruption detected during '%s' in block %p allocated from %p", string, (void *)(z+1), z->allocationLocation);
			}
		}

		if (i++ > z_count * 2)
			Com_Error (ERR_DIE, "Z_Verify: memory chain state corrupted during '%s'", string);
	}
}

RESTRICT void * EXPORT Z_TagMallocDebug (int size, int tag)
{
	zhead_t	*z;

	Z_Verify ("Z_TagMallocDebug: START ALLOCATION OF %d BYTES FOR TAG %d (%s)", size, tag, tag < TAGMALLOC_MAX_TAGS ?  tagmalloc_tags[tag].name : "UNKNOWN TAG");

	if (size <= 0 && tag < TAGMALLOC_MAX_TAGS)
		Com_Error (ERR_DIE, "Z_TagMalloc: trying to allocate %d bytes!", size);

	size = size + sizeof(zhead_t);

	if (size > 0x40000000)
		Com_Error (ERR_DIE, "Z_TagMalloc: trying to allocate %d bytes!", size);

	size++;

	z = malloc(size);

	if (!z)
		Com_Error (ERR_DIE, "Z_TagMalloc: Out of memory. Couldn't allocate %i bytes for %s (already %li bytes in %li blocks)", size, tagmalloc_tags[tag].name, z_bytes, z_count);

	//memset (z, 0xCC, size);

	(*(byte **)&z)[size-1] = 0xCC;

	z_count++;

	if (tag < TAGMALLOC_MAX_TAGS)
		tagmalloc_tags[tag].allocs++;

	z_bytes += size;

	z->magic = Z_MAGIC_DEBUG;
	z->tag = tag;
	z->size = size;

#if defined _WIN32
	z->allocationLocation = _ReturnAddress ();
#elif defined LINUX
	z->allocationLocation = __builtin_return_address (0);
#else
	//FIXME: other OSes/CCs
	z->allocationLocation = 0;
#endif

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	Z_Verify ("Z_TagMallocDebug: END ALLOCATION OF %d BYTES FOR TAG %d (%s)", size, tag, tag < TAGMALLOC_MAX_TAGS ?  tagmalloc_tags[tag].name : "UNKNOWN TAG");

	return (void *)(z+1);
}

/*
========================
Z_TagMalloc
========================
*/
RESTRICT void * EXPORT Z_TagMallocRelease (int size, int tag)
{
	zhead_t	*z;

	//malloc can crash if negative size is passed, woops.
	if (size < 0)
		Com_Error (ERR_DIE, "Z_TagMalloc: Illegal allocation size of %d bytes from %p for tag %d", size,
#if defined _WIN32
		_ReturnAddress (),
#elif defined LINUX
		__builtin_return_address (0), 
#else
		NULL,
#endif
		tag);

	size = size + sizeof(zhead_t);
	z = malloc(size);

	if (!z)
		Com_Error (ERR_DIE, "Z_TagMalloc: Out of memory. Couldn't allocate %i bytes for tag %d from %p (already %li bytes in %li blocks)", size, tag,
#if defined _WIN32
		_ReturnAddress (),
#elif defined LINUX
		__builtin_return_address (0), 
#else
		NULL,
#endif		
		z_bytes, z_count);

	z_count++;

	if ((uint32)tag < TAGMALLOC_MAX_TAGS)
		tagmalloc_tags[tag].allocs++;

	z_bytes += size;

#if defined _WIN32
	z->allocationLocation = _ReturnAddress ();
#elif defined LINUX
	z->allocationLocation = __builtin_return_address (0);
#else
	//FIXME: other OSes/CCs
	z->allocationLocation = 0;
#endif

	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	return (void *)(z+1);
}

RESTRICT void * EXPORT Z_TagMallocGame (int size, int tag)
{
	z_memloc_t	*loc, *last, *newentry;
	byte		*b;

	void		*retAddr;

#if defined _WIN32
	retAddr = _ReturnAddress ();
#elif defined LINUX
	retAddr = __builtin_return_address (0);
#else
	//FIXME: other OSes/CCs
	retAddr = 0;
#endif

	//aieeeee.
	if (z_buggygame->intvalue)
		size *= z_buggygame->intvalue;

	if (!size)
	{
		if (z_buggygame->intvalue)
		{
			Com_Printf ("ERROR: Game DLL tried to allocate 0 bytes at %p. Giving it 1 byte of 0x00\n", LOG_ERROR|LOG_SERVER|LOG_GAME, retAddr);
			size = 1;
		}
		else
		{
			Com_Error (ERR_DIE, "Game DLL tried to allocate 0 bytes from code at %p!", retAddr);
		}
	}

	b = Z_TagMalloc (size+4, tag);

	memset (b, 0, size);
	*(int *)(b + size) = 0xFDFEFDFE;

	if (tag == TAG_LEVEL)
		z_level_allocs++;
	else if (tag == TAG_GAME)
		z_game_allocs++;

	loc = &z_game_locations;
	last = loc->next;
	newentry = malloc (sizeof(*loc));
	if (!newentry)
		Com_Error (ERR_DIE, "Z_TagMallocGame: Out of memory.");

	newentry->address = b;
	newentry->size = size;
	newentry->next = last;
	newentry->time = curtime;
	newentry->allocationLocation = retAddr;
	loc->next = newentry;

	return (void *)b;
}

void EXPORT Z_FreeGame (void *buf)
{
	z_memloc_t	*loc, *last;

	void		*retAddr;

#if defined _WIN32
	retAddr = _ReturnAddress ();
#elif defined LINUX
	retAddr = __builtin_return_address (0);
#else
	//FIXME: other OSes/CCs
	retAddr = 0;
#endif

	loc = last = &z_game_locations;

	while (loc->next)
	{
		loc = loc->next;
		if (buf == loc->address)
		{
			if (*(int *)((byte *)buf + loc->size) != 0xFDFEFDFE)
			{
				Com_Printf ("Memory corruption detected within the Game DLL. Please contact the mod author and inform them that they are not managing dynamically allocated memory correctly.\n", LOG_GENERAL);
				Com_Error (ERR_DIE, "Z_FreeGame: Game DLL corrupted a memory block of size %d at %p (allocated %u ms ago from code at %p), detected during free at %p", loc->size, loc->address, curtime - loc->time, loc->allocationLocation, retAddr);
			}
			free_from_game = true;
			Z_Free (buf);
			free_from_game = false;

			last->next = loc->next;
			free (loc);
			return;
		}
		last = loc;
	}

	if (z_buggygame->intvalue)
	{
		Com_Printf ("ERROR: Game DLL tried to free non-existent/freed memory at %p from code at %p, ignored.", LOG_ERROR|LOG_SERVER|LOG_GAME, buf, retAddr);
	}
	else
	{
		Com_Printf ("Memory management problem detected within the Game DLL. Please contact the mod author and inform them that they are not managing dynamically allocated memory correctly.\n", LOG_GENERAL);
		Com_Error (ERR_DIE, "Z_FreeGame: Game DLL tried to free non-existent/freed memory at %p from code at %p", buf, retAddr);
	}
}

void EXPORT Z_FreeTagsGame (int tag)
{
	zhead_t	*z, *next;

	z_memloc_t	*loc, *last;

	loc = last = &z_game_locations;

	while (loc->next)
	{
		loc = loc->next;

		if (*(int *)((byte *)loc->address + loc->size) != 0xFDFEFDFE)
		{
			Com_Printf ("Memory corruption detected within the Game DLL. Please contact the mod author and inform them that they are not managing dynamically allocated memory correctly.\n", LOG_GENERAL);
			Com_Error (ERR_DIE, "Z_FreeTagsGame: Game DLL corrupted a memory block of size %d at %p (allocated %u ms ago from code at %p)", loc->size, loc->address, curtime - loc->time, loc->allocationLocation);
		}
	}

	for (z=z_chain.next ; z != &z_chain ; z=next)
	{
		next = z->next;
		if (z->tag == tag)
			Z_FreeGame ((void *)(z+1));
	}

	Z_Verify (va("Z_FreeTags %d (GAME): END", tag));
}


/*
========================
Z_Malloc
========================
*/

/*void *Z_Malloc (int size)
{
	return Z_TagMalloc (size, 0);
}*/


//============================================================================

static byte chktbl[1024] = {
0x84, 0x47, 0x51, 0xc1, 0x93, 0x22, 0x21, 0x24, 0x2f, 0x66, 0x60, 0x4d, 0xb0, 0x7c, 0xda,
0x88, 0x54, 0x15, 0x2b, 0xc6, 0x6c, 0x89, 0xc5, 0x9d, 0x48, 0xee, 0xe6, 0x8a, 0xb5, 0xf4,
0xcb, 0xfb, 0xf1, 0x0c, 0x2e, 0xa0, 0xd7, 0xc9, 0x1f, 0xd6, 0x06, 0x9a, 0x09, 0x41, 0x54,
0x67, 0x46, 0xc7, 0x74, 0xe3, 0xc8, 0xb6, 0x5d, 0xa6, 0x36, 0xc4, 0xab, 0x2c, 0x7e, 0x85,
0xa8, 0xa4, 0xa6, 0x4d, 0x96, 0x19, 0x19, 0x9a, 0xcc, 0xd8, 0xac, 0x39, 0x5e, 0x3c, 0xf2,
0xf5, 0x5a, 0x72, 0xe5, 0xa9, 0xd1, 0xb3, 0x23, 0x82, 0x6f, 0x29, 0xcb, 0xd1, 0xcc, 0x71,
0xfb, 0xea, 0x92, 0xeb, 0x1c, 0xca, 0x4c, 0x70, 0xfe, 0x4d, 0xc9, 0x67, 0x43, 0x47, 0x94,
0xb9, 0x47, 0xbc, 0x3f, 0x01, 0xab, 0x7b, 0xa6, 0xe2, 0x76, 0xef, 0x5a, 0x7a, 0x29, 0x0b,
0x51, 0x54, 0x67, 0xd8, 0x1c, 0x14, 0x3e, 0x29, 0xec, 0xe9, 0x2d, 0x48, 0x67, 0xff, 0xed,
0x54, 0x4f, 0x48, 0xc0, 0xaa, 0x61, 0xf7, 0x78, 0x12, 0x03, 0x7a, 0x9e, 0x8b, 0xcf, 0x83,
0x7b, 0xae, 0xca, 0x7b, 0xd9, 0xe9, 0x53, 0x2a, 0xeb, 0xd2, 0xd8, 0xcd, 0xa3, 0x10, 0x25,
0x78, 0x5a, 0xb5, 0x23, 0x06, 0x93, 0xb7, 0x84, 0xd2, 0xbd, 0x96, 0x75, 0xa5, 0x5e, 0xcf,
0x4e, 0xe9, 0x50, 0xa1, 0xe6, 0x9d, 0xb1, 0xe3, 0x85, 0x66, 0x28, 0x4e, 0x43, 0xdc, 0x6e,
0xbb, 0x33, 0x9e, 0xf3, 0x0d, 0x00, 0xc1, 0xcf, 0x67, 0x34, 0x06, 0x7c, 0x71, 0xe3, 0x63,
0xb7, 0xb7, 0xdf, 0x92, 0xc4, 0xc2, 0x25, 0x5c, 0xff, 0xc3, 0x6e, 0xfc, 0xaa, 0x1e, 0x2a,
0x48, 0x11, 0x1c, 0x36, 0x68, 0x78, 0x86, 0x79, 0x30, 0xc3, 0xd6, 0xde, 0xbc, 0x3a, 0x2a,
0x6d, 0x1e, 0x46, 0xdd, 0xe0, 0x80, 0x1e, 0x44, 0x3b, 0x6f, 0xaf, 0x31, 0xda, 0xa2, 0xbd,
0x77, 0x06, 0x56, 0xc0, 0xb7, 0x92, 0x4b, 0x37, 0xc0, 0xfc, 0xc2, 0xd5, 0xfb, 0xa8, 0xda,
0xf5, 0x57, 0xa8, 0x18, 0xc0, 0xdf, 0xe7, 0xaa, 0x2a, 0xe0, 0x7c, 0x6f, 0x77, 0xb1, 0x26,
0xba, 0xf9, 0x2e, 0x1d, 0x16, 0xcb, 0xb8, 0xa2, 0x44, 0xd5, 0x2f, 0x1a, 0x79, 0x74, 0x87,
0x4b, 0x00, 0xc9, 0x4a, 0x3a, 0x65, 0x8f, 0xe6, 0x5d, 0xe5, 0x0a, 0x77, 0xd8, 0x1a, 0x14,
0x41, 0x75, 0xb1, 0xe2, 0x50, 0x2c, 0x93, 0x38, 0x2b, 0x6d, 0xf3, 0xf6, 0xdb, 0x1f, 0xcd,
0xff, 0x14, 0x70, 0xe7, 0x16, 0xe8, 0x3d, 0xf0, 0xe3, 0xbc, 0x5e, 0xb6, 0x3f, 0xcc, 0x81,
0x24, 0x67, 0xf3, 0x97, 0x3b, 0xfe, 0x3a, 0x96, 0x85, 0xdf, 0xe4, 0x6e, 0x3c, 0x85, 0x05,
0x0e, 0xa3, 0x2b, 0x07, 0xc8, 0xbf, 0xe5, 0x13, 0x82, 0x62, 0x08, 0x61, 0x69, 0x4b, 0x47,
0x62, 0x73, 0x44, 0x64, 0x8e, 0xe2, 0x91, 0xa6, 0x9a, 0xb7, 0xe9, 0x04, 0xb6, 0x54, 0x0c,
0xc5, 0xa9, 0x47, 0xa6, 0xc9, 0x08, 0xfe, 0x4e, 0xa6, 0xcc, 0x8a, 0x5b, 0x90, 0x6f, 0x2b,
0x3f, 0xb6, 0x0a, 0x96, 0xc0, 0x78, 0x58, 0x3c, 0x76, 0x6d, 0x94, 0x1a, 0xe4, 0x4e, 0xb8,
0x38, 0xbb, 0xf5, 0xeb, 0x29, 0xd8, 0xb0, 0xf3, 0x15, 0x1e, 0x99, 0x96, 0x3c, 0x5d, 0x63,
0xd5, 0xb1, 0xad, 0x52, 0xb8, 0x55, 0x70, 0x75, 0x3e, 0x1a, 0xd5, 0xda, 0xf6, 0x7a, 0x48,
0x7d, 0x44, 0x41, 0xf9, 0x11, 0xce, 0xd7, 0xca, 0xa5, 0x3d, 0x7a, 0x79, 0x7e, 0x7d, 0x25,
0x1b, 0x77, 0xbc, 0xf7, 0xc7, 0x0f, 0x84, 0x95, 0x10, 0x92, 0x67, 0x15, 0x11, 0x5a, 0x5e,
0x41, 0x66, 0x0f, 0x38, 0x03, 0xb2, 0xf1, 0x5d, 0xf8, 0xab, 0xc0, 0x02, 0x76, 0x84, 0x28,
0xf4, 0x9d, 0x56, 0x46, 0x60, 0x20, 0xdb, 0x68, 0xa7, 0xbb, 0xee, 0xac, 0x15, 0x01, 0x2f,
0x20, 0x09, 0xdb, 0xc0, 0x16, 0xa1, 0x89, 0xf9, 0x94, 0x59, 0x00, 0xc1, 0x76, 0xbf, 0xc1,
0x4d, 0x5d, 0x2d, 0xa9, 0x85, 0x2c, 0xd6, 0xd3, 0x14, 0xcc, 0x02, 0xc3, 0xc2, 0xfa, 0x6b,
0xb7, 0xa6, 0xef, 0xdd, 0x12, 0x26, 0xa4, 0x63, 0xe3, 0x62, 0xbd, 0x56, 0x8a, 0x52, 0x2b,
0xb9, 0xdf, 0x09, 0xbc, 0x0e, 0x97, 0xa9, 0xb0, 0x82, 0x46, 0x08, 0xd5, 0x1a, 0x8e, 0x1b,
0xa7, 0x90, 0x98, 0xb9, 0xbb, 0x3c, 0x17, 0x9a, 0xf2, 0x82, 0xba, 0x64, 0x0a, 0x7f, 0xca,
0x5a, 0x8c, 0x7c, 0xd3, 0x79, 0x09, 0x5b, 0x26, 0xbb, 0xbd, 0x25, 0xdf, 0x3d, 0x6f, 0x9a,
0x8f, 0xee, 0x21, 0x66, 0xb0, 0x8d, 0x84, 0x4c, 0x91, 0x45, 0xd4, 0x77, 0x4f, 0xb3, 0x8c,
0xbc, 0xa8, 0x99, 0xaa, 0x19, 0x53, 0x7c, 0x02, 0x87, 0xbb, 0x0b, 0x7c, 0x1a, 0x2d, 0xdf,
0x48, 0x44, 0x06, 0xd6, 0x7d, 0x0c, 0x2d, 0x35, 0x76, 0xae, 0xc4, 0x5f, 0x71, 0x85, 0x97,
0xc4, 0x3d, 0xef, 0x52, 0xbe, 0x00, 0xe4, 0xcd, 0x49, 0xd1, 0xd1, 0x1c, 0x3c, 0xd0, 0x1c,
0x42, 0xaf, 0xd4, 0xbd, 0x58, 0x34, 0x07, 0x32, 0xee, 0xb9, 0xb5, 0xea, 0xff, 0xd7, 0x8c,
0x0d, 0x2e, 0x2f, 0xaf, 0x87, 0xbb, 0xe6, 0x52, 0x71, 0x22, 0xf5, 0x25, 0x17, 0xa1, 0x82,
0x04, 0xc2, 0x4a, 0xbd, 0x57, 0xc6, 0xab, 0xc8, 0x35, 0x0c, 0x3c, 0xd9, 0xc2, 0x43, 0xdb,
0x27, 0x92, 0xcf, 0xb8, 0x25, 0x60, 0xfa, 0x21, 0x3b, 0x04, 0x52, 0xc8, 0x96, 0xba, 0x74,
0xe3, 0x67, 0x3e, 0x8e, 0x8d, 0x61, 0x90, 0x92, 0x59, 0xb6, 0x1a, 0x1c, 0x5e, 0x21, 0xc1,
0x65, 0xe5, 0xa6, 0x34, 0x05, 0x6f, 0xc5, 0x60, 0xb1, 0x83, 0xc1, 0xd5, 0xd5, 0xed, 0xd9,
0xc7, 0x11, 0x7b, 0x49, 0x7a, 0xf9, 0xf9, 0x84, 0x47, 0x9b, 0xe2, 0xa5, 0x82, 0xe0, 0xc2,
0x88, 0xd0, 0xb2, 0x58, 0x88, 0x7f, 0x45, 0x09, 0x67, 0x74, 0x61, 0xbf, 0xe6, 0x40, 0xe2,
0x9d, 0xc2, 0x47, 0x05, 0x89, 0xed, 0xcb, 0xbb, 0xb7, 0x27, 0xe7, 0xdc, 0x7a, 0xfd, 0xbf,
0xa8, 0xd0, 0xaa, 0x10, 0x39, 0x3c, 0x20, 0xf0, 0xd3, 0x6e, 0xb1, 0x72, 0xf8, 0xe6, 0x0f,
0xef, 0x37, 0xe5, 0x09, 0x33, 0x5a, 0x83, 0x43, 0x80, 0x4f, 0x65, 0x2f, 0x7c, 0x8c, 0x6a,
0xa0, 0x82, 0x0c, 0xd4, 0xd4, 0xfa, 0x81, 0x60, 0x3d, 0xdf, 0x06, 0xf1, 0x5f, 0x08, 0x0d,
0x6d, 0x43, 0xf2, 0xe3, 0x11, 0x7d, 0x80, 0x32, 0xc5, 0xfb, 0xc5, 0xd9, 0x27, 0xec, 0xc6,
0x4e, 0x65, 0x27, 0x76, 0x87, 0xa6, 0xee, 0xee, 0xd7, 0x8b, 0xd1, 0xa0, 0x5c, 0xb0, 0x42,
0x13, 0x0e, 0x95, 0x4a, 0xf2, 0x06, 0xc6, 0x43, 0x33, 0xf4, 0xc7, 0xf8, 0xe7, 0x1f, 0xdd,
0xe4, 0x46, 0x4a, 0x70, 0x39, 0x6c, 0xd0, 0xed, 0xca, 0xbe, 0x60, 0x3b, 0xd1, 0x7b, 0x57,
0x48, 0xe5, 0x3a, 0x79, 0xc1, 0x69, 0x33, 0x53, 0x1b, 0x80, 0xb8, 0x91, 0x7d, 0xb4, 0xf6,
0x17, 0x1a, 0x1d, 0x5a, 0x32, 0xd6, 0xcc, 0x71, 0x29, 0x3f, 0x28, 0xbb, 0xf3, 0x5e, 0x71,
0xb8, 0x43, 0xaf, 0xf8, 0xb9, 0x64, 0xef, 0xc4, 0xa5, 0x6c, 0x08, 0x53, 0xc7, 0x00, 0x10,
0x39, 0x4f, 0xdd, 0xe4, 0xb6, 0x19, 0x27, 0xfb, 0xb8, 0xf5, 0x32, 0x73, 0xe5, 0xcb, 0x32,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00
};

/*
====================
COM_BlockSequenceCRCByte

For proxy protecting
====================
*/
byte	COM_BlockSequenceCRCByte (byte *base, int length, int sequence)
{
	int				n;
	int				x;
	byte			*p;
	byte			chkb[60 + 4];
	uint16			crc;
	byte			r;


	if (sequence < 0)
		Sys_Error("sequence < 0, this shouldn't happen\n");

	p = chktbl + (sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block(chkb, length);

	for (x=0, n=0; n<length; n++)
		x += chkb[n];

	r = (crc ^ x) & 0xff;

	return r;
}

#ifdef USE_OPENSSL
uint32 Com_BlockChecksum (void *buffer, int length)
{
	int			digest[4];
	uint32		val;
	MD4_CTX		ctx;

	MD4_Init (&ctx);
	MD4_Update (&ctx, (unsigned char *)buffer, length);
	MD4_Final ( (unsigned char *)digest, &ctx);
	
	val = digest[0] ^ digest[1] ^ digest[2] ^ digest[3];

	return val;
}
#endif

//========================================================

/*float	frand(void)
{
	return (randomMT()&32767)* (1.0/32767);
}

float	crand(void)
{
	return (randomMT()&32767)* (2.0/32767) - 1;
}*/

void Key_Init (void);
void SCR_EndLoadingPlaque (void);

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
void Com_Error_f (void)
{
	Com_Error (ERR_FATAL, "%s", Cmd_Argv(1));
}


/*
=================
Qcommon_Init
=================
*/

void Q_NullFunc(void)
{
}

void Msg_Stats_f (void)
{
#ifndef NPROFILE
	int		total;
	int		i, j;
	int		num;
	int		sum;

	total = msg_malloc_hits + msg_local_hits;

	Com_Printf ("malloc: %d (%.2f%%), local: %d (%.2f%%)\n", LOG_GENERAL,
		msg_malloc_hits, ((float)msg_malloc_hits / (float)total) * 100.0f,
		msg_local_hits, ((float)msg_local_hits / (float)total) * 100.0f);
	
	Com_Printf ("byte breakdown:\n", LOG_GENERAL);

	total = 0;
	num = 0;

	for (i = 0; i < sizeof(messageSizes) / sizeof(messageSizes[0]); i += 10)
	{
		sum = 0;
		for (j = i; j < i+ 10; j++)
		{
			sum += messageSizes[j];
			total += j * messageSizes[j];
			num += messageSizes[j];
		}
		Com_Printf ("%i-%d: %d\n", LOG_GENERAL, i, i + 10, sum);
	}
	Com_Printf ("mean: %.2f\n", LOG_GENERAL, (float)total / (float)num);
#else
	Com_Printf ("This binary was built with NPROFILE, no stats available.\n", LOG_GENERAL);
#endif
}

void _z_debug_changed (cvar_t *cvar, char *o, char *n)
{
	if (cvar->intvalue)
	{
		Z_TagMalloc = Z_TagMallocDebug;
		Z_Free = Z_FreeDebug;
	}
	else
	{
		Z_TagMalloc = Z_TagMallocRelease;
		Z_Free = Z_FreeRelease;
	}

	Com_Printf ("Z_Debug: Intensive memory checking %s.\n", LOG_GENERAL, cvar->intvalue ? "enabled" : "disabled");
}

void _logfile_changed (cvar_t *cvar, char *o, char *n)
{
	if (cvar->intvalue == 0)
	{
		if (logfile)
		{
			fclose (logfile);
			logfile = NULL;
		}
	}
}

void Qcommon_Init (int argc, char **argv)
{
	if (setjmp (abortframe) )
		Sys_Error ("Error during initialization");

	seedMT((uint32)time(0));

	SZ_Init (&msgbuff, message_buff, sizeof(message_buff));

	Z_Free = Z_FreeRelease;
	Z_TagMalloc = Z_TagMallocRelease;

	z_chain.next = z_chain.prev = &z_chain;

	uninitialized_cvar.string = "";

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	COM_InitArgv (argc, argv);

	Swap_Init ();
	Cbuf_Init ();

	Cmd_Init ();
	Cvar_Init ();

	z_debug = Cvar_Get ("z_debug", "0", 0);
	z_buggygame = Cvar_Get ("z_buggygame", "0", 0);
	z_allowcorruption = Cvar_Get ("z_allowcorruption", "0", 0);

	cl_quietstartup = Cvar_Get ("cl_quietstartup", "1", 0);

	z_debug->changed = _z_debug_changed;
	if (z_debug->intvalue)
	{
		Z_TagMalloc = Z_TagMallocDebug;
		Z_Free = Z_FreeDebug;
	}

#ifndef DEDICATED_ONLY
	Key_Init ();
#else
	//r1: stub out these so configs don't spam unknown cmd
	Cmd_AddCommand ("bind",Q_NullFunc);
	Cmd_AddCommand ("unbind",Q_NullFunc);
	Cmd_AddCommand ("unbindall",Q_NullFunc);
	Cmd_AddCommand ("bindlist",Q_NullFunc);
#endif

	Cmd_AddCommand ("msg_stats", Msg_Stats_f);
	Cmd_AddCommand ("processtimes", Sys_ProcessTimes_f);
	Cmd_AddCommand ("spinstats", Sys_Spinstats_f);

	// we need to add the early commands twice, because
	// a basedir or cddir needs to be set before execing
	// config files, but we want other parms to override
	// the settings of the config files
	Cbuf_AddEarlyCommands (false);
	Cbuf_Execute ();

	FS_InitFilesystem ();

	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_AddText ("exec config.cfg\n");

	Cbuf_AddEarlyCommands (true);
	Cbuf_Execute ();

	//
	// init commands and vars
	//
    Cmd_AddCommand ("z_stats", Z_Stats_f);
    Cmd_AddCommand ("error", Com_Error_f);

	host_speeds = Cvar_Get ("host_speeds", "0", 0);
	log_stats = Cvar_Get ("log_stats", "0", 0);
	timescale = Cvar_Get ("timescale", "1", 0);
	fixedtime = Cvar_Get ("fixedtime", "0", 0);

	logfile_active = Cvar_Get ("logfile", "0", 0);
	logfile_timestamp = Cvar_Get ("logfile_timestamp", "1", 0);
	logfile_timestamp_format = Cvar_Get ("logfile_timestamp_format", "[%Y-%m-%d %H:%M]", 0);
	logfile_name = Cvar_Get ("logfile_name", "qconsole.log", 0);
	logfile_filterlevel = Cvar_Get ("logfile_filterlevel", "0", 0);
	logfile_active->changed = _logfile_changed;

	con_filterlevel = Cvar_Get ("con_filterlevel", "0", 0);

#ifndef DEDICATED_ONLY
	showtrace = Cvar_Get ("showtrace", "0", 0);
#endif
#ifndef NO_SERVER
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get ("dedicated", "1", CVAR_NOSET);
#else
	dedicated = Cvar_Get ("dedicated", "0", CVAR_NOSET);
#endif
#endif

	sys_loopstyle = Cvar_Get ("sys_loopstyle", "1", 0);
	
	err_fatal = Cvar_Get ("err_fatal", "0", 0);

	//s = va("R1Q2 %s %s %s %s", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
	Cvar_Get ("version", R1Q2_VERSION_STRING, CVAR_SERVERINFO|CVAR_NOSET);

#ifndef NO_SERVER
	if (dedicated->intvalue)
		Cmd_AddCommand ("quit", Com_Quit);
#endif

	dbg_unload = Cvar_Get ("dbg_unload", "1", 0);
	dbg_crash_on_fatal_error = Cvar_Get ("dbg_crash_on_fatal_error", "0", 0);

	Sys_Init ();

	NET_Init ();
	Netchan_Init ();

#ifndef NO_SERVER
	SV_Init ();
#endif

	Com_Printf ("====== Quake2 Initialized ======\n", LOG_GENERAL);	
	Com_Printf ("R1Q2 build " BUILD ", compiled " __DATE__ ".\n"
				"http://www.r1ch.net/stuff/r1q2/\n"
				BUILDSTRING " " CPUSTRING " (%s)\n\n", LOG_GENERAL, binary_name);

#ifndef DEDICATED_ONLY
	CL_Init ();
#endif

	// add + commands from command line
	if (!Cbuf_AddLateCommands ())
	{	
		// if the user didn't give any commands, run default action
#ifndef NO_SERVER
		if (!dedicated->intvalue)
		{
#endif

#ifndef DEDICATED_ONLY
			Cbuf_AddText ("toggleconsole\n");

			Com_Printf (
				"Welcome to R1Q2.\n"
				"\n"
				"  Press <ESC> to open the menu.\n"
				"  Type connect <host> to connect to a server.\n"
				"  View the readme at http://www.r1ch.net/forum/index.php?board=8.0\n"
				"\n", LOG_GENERAL);

			Sys_UpdateURLMenu ("http://www.r1ch.net/forum/index.php?board=8.0");
#endif

#ifndef NO_SERVER
		}
		else
			Cbuf_AddText ("dedicated_start\n");
#endif
		Cbuf_Execute ();
	}
#ifndef DEDICATED_ONLY
	else
	{	
		// the user asked for something explicit
		// so drop the loading plaque
		SCR_EndLoadingPlaque ();
	}
#endif

	Cbuf_Execute ();

	//MSG_NUinitHuffman ();

	//ugly
	q2_initialized = true;
	logfile_timestamp_format->flags |= CVAR_NOSET;
}

/*
=================
Qcommon_Frame
=================
*/
void Qcommon_Frame (int msec)
{
#ifndef NO_SERVER
	char *s;
#endif

#ifndef DEDICATED_ONLY
	int		time_before = 0, time_between = 0, time_after;
#endif

	if (setjmp (abortframe) )
		return;			// an ERR_DROP was thrown

	//Com_Printf ("frame time: %d ms\n", LOG_GENERAL, msec);

	/*if ( log_stats->modified )
	{
		log_stats->modified = false;
		if ( log_stats->intvalue )
		{
			if ( log_stats_file )
			{
				fclose( log_stats_file );
				log_stats_file = 0;
			}
			log_stats_file = fopen( "stats.log", "w" );
			if ( log_stats_file )
				fprintf( log_stats_file, "entities,dlights,parts,frame time\n" );
		}
		else
		{
			if ( log_stats_file )
			{
				fclose( log_stats_file );
				log_stats_file = 0;
			}
		}
	}*/

	if (fixedtime->intvalue)
	{
		msec = fixedtime->intvalue;
	}
	else
	{
		msec = (int)(msec * timescale->value);

		//r1: allow server to run zero msec frames to avoid spinloop
		/*if (!dedicated->intvalue)
		{
			if (msec < 1)
				msec = 1;
		}*/
	}

#ifndef DEDICATED_ONLY
	if (showtrace->intvalue)
	{
		extern	int c_traces, c_brush_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  %4i points\n", LOG_GENERAL, c_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}
#endif

#ifndef NO_SERVER
#ifndef DEDICATED_ONLY
	if (dedicated->intvalue)
	{
#endif
		do
		{
			s = Sys_ConsoleInput ();
			if (s)
				Cbuf_AddText (va("%s\n",s));
			Cbuf_Execute();
		} while (s);
#ifndef DEDICATED_ONLY
	}
#endif
#endif

	//Cbuf_Execute ();

#ifndef DEDICATED_ONLY
	if (host_speeds->intvalue)
		time_before = Sys_Milliseconds ();
#endif

#ifndef NO_SERVER
	SV_Frame (msec);
#endif

#ifndef DEDICATED_ONLY
	if (host_speeds->intvalue)
		time_between = Sys_Milliseconds ();

	CL_Frame (msec);

	if (host_speeds->intvalue) {
		int			all, sv, gm, cl, rf;

		time_after = Sys_Milliseconds ();

		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Com_Printf ("all:%3i sv:%3i gm:%3i cl:%3i rf:%3i\n", LOG_GENERAL,
			all, sv, gm, cl, rf);
	}
#endif

}

#ifndef NO_ZLIB
int ZLibDecompress (byte *in, int inlen, byte *out, int outlen, int wbits)
{
	z_stream zs;
	int result;

	memset (&zs, 0, sizeof(zs));

	zs.next_in = in;
	zs.avail_in = 0;

	zs.next_out = out;
	zs.avail_out = outlen;

	result = inflateInit2(&zs, wbits);
	if (result != Z_OK)
	{
		Com_Error (ERR_DROP, "ZLib data error! Error %d on inflateInit.\nMessage: %s", result, zs.msg);
		return 0;
	}

	zs.avail_in = inlen;

	result = inflate(&zs, Z_FINISH);
	if (result != Z_STREAM_END)
	{
		Com_Error (ERR_DROP, "ZLib data error! Error %d on inflate.\nMessage: %s", result, zs.msg);
		zs.total_out = 0;
	}

	result = inflateEnd(&zs);
	if (result != Z_OK)
	{
		Com_Error (ERR_DROP, "ZLib data error! Error %d on inflateEnd.\nMessage: %s", result, zs.msg);
		return 0;
	}

	return zs.total_out;
}

int ZLibCompressChunk(byte *in, int len_in, byte *out, int len_out, int method, int wbits)
{
	z_stream zs;
	int result;

	zs.next_in = in;
	zs.avail_in = len_in;
	zs.total_in = 0;

	zs.next_out = out;
	zs.avail_out = len_out;
	zs.total_out = 0;

	zs.msg = NULL;
	zs.state = NULL;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = NULL;

	zs.data_type = Z_BINARY;
	zs.adler = 0;
	zs.reserved = 0;

	result = deflateInit2 (&zs, method, Z_DEFLATED, wbits, 9, Z_DEFAULT_STRATEGY);
	if (result != Z_OK)
		return -1;

	result = deflate(&zs, Z_FINISH);
	if (result != Z_STREAM_END)
		return -1;

	result = deflateEnd(&zs);
	if (result != Z_OK)
		return -1;

	return zs.total_out;
}
#endif

void StripHighBits (char *string, int highbits)
{
	byte		high;
	byte		c;
	char		*p;

	p = string;

	if (highbits)
		high = 127;
	else
		high = 255;

	while (string[0])
	{
		c = *(string++);

		if (c >= 32 && c <= high)
			*p++ = c;
	}

	p[0] = '\0';
}

qboolean isvalidchar (int c)
{
	if (!isalnum(c) && c != '_' && c != '-')
		return false;
	return true;
}

void ExpandNewLines (char *string)
{
	char *q = string;
	char *s = q;

	if (!string[0])
		return;

	while (*(q+1))
	{
		if (*q == '\\' && *(q+1) == 'n')
		{
			*s++ = '\n';
			q++;
		}
		else
		{
			*s++ = *q;
		}
		q++;

		//crashfix, check if we reached eol on an expansion.
		if (!*q)
			break;
	}

	if (*q)
		*s++ = *q;
	*s = '\0';
}

char *StripQuotes (char *string)
{
	size_t	i;

	if (!string[0])
		return string;

	i = strlen(string);

	if (string[0] == '"' && string[i-1] == '"')
	{
		string[i-1] = 0;
		return string + 1;
	}

	return string;
}

const char *MakePrintable (const void *subject, size_t numchars)
{
	int			len;
	static char printable[4096];
	char		tmp[8];
	char		*p;
	const byte	*s;

	if (!subject)
	{
		strcpy (printable, "(null)");
		return printable;
	}

	s = (const byte *)subject;
	p = printable;
	len = 0;

	if (!numchars)
		numchars = strlen((const char *) s);

	while (numchars--)
	{
		if (isprint(s[0]))
		{
			*p++ = s[0];
			len++;
		}
		else
		{
			sprintf (tmp, "%.3d", s[0]);
			*p++ = '\\';
			*p++ = tmp[0];
			*p++ = tmp[1];
			*p++ = tmp[2];
			len += 4;
		}

		if (len >= sizeof(printable)-5)
			break;

		s++;
	}

	printable[len] = 0;
	return printable;
}

/*
=================
Qcommon_Shutdown
=================
*/
void Qcommon_Shutdown (void)
{
}

void Z_CheckGameLeaks (void)
{
	z_memloc_t	*loc, *last;

	loc = last = &z_game_locations;

	if (loc->next)
	{
		Com_Printf ("Memory leak detected in Game DLL. Leaked blocks: ", LOG_GENERAL|LOG_WARNING);

		while (loc->next)
		{
			loc = loc->next;
			Com_Printf ("%p (%d bytes)%s", LOG_GENERAL|LOG_WARNING, loc->address, loc->size, loc->next ? ", " : "");
		}
		Com_Printf ("\n", LOG_GENERAL|LOG_WARNING);
	}
}

