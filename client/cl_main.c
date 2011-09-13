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
// cl_main.c  -- client main loop

#include "client.h"

#ifndef __TIMESTAMP__
#define __TIMESTAMP__ __DATE__ " " __TIME__
#endif

int deferred_model_index;

extern cvar_t	*qport;
extern cvar_t	*vid_ref;

typedef struct incoming_s
{
	netadr_t	remote;
	int			type;
	uint32		time;
} incoming_t;

#define	CL_UNDEF				0
#define	CL_RCON					1
#define	CL_SERVER_INFO			2
#define	CL_SERVER_STATUS		3
#define	CL_SERVER_STATUS_FULL	4
#define CL_MASTER_QUERY			5

//list of addresses other than remote that can send us connectionless and for what purpose
static incoming_t	incoming_allowed[16];
static int			incoming_allowed_index;

typedef struct ignore_s
{
	struct ignore_s	*next;
	char			*text;
} ignore_t;

ignore_t	cl_ignores;

cvar_t	*freelook;

cvar_t	*adr0;
cvar_t	*adr1;
cvar_t	*adr2;
cvar_t	*adr3;
cvar_t	*adr4;
cvar_t	*adr5;
cvar_t	*adr6;
cvar_t	*adr7;
cvar_t	*adr8;

#ifdef CL_STEREO_SUPPORT
cvar_t	*cl_stereo_separation;
cvar_t	*cl_stereo;
#endif

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_noskins;
//cvar_t	*cl_autoskins;
cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
cvar_t	*cl_backlerp;
cvar_t	*r_maxfps;
cvar_t	*cl_maxfps;
cvar_t	*cl_async;
cvar_t	*cl_smoothsteps;
cvar_t	*cl_instantpacket;

cvar_t	*cl_gun;

cvar_t	*cl_add_particles;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_entities;
cvar_t	*cl_add_blend;

cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;

cvar_t	*cl_paused;
cvar_t	*cl_timedemo;

//r1: filter high bits
cvar_t	*cl_filterchat;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*sensitivity;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_lightlevel;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*skin;
cvar_t	*rate;
cvar_t	*fov;
cvar_t	*msg;
cvar_t	*hand;
cvar_t	*gender;
cvar_t	*gender_auto;

cvar_t	*cl_vwep;

//r1ch
//cvar_t	*cl_defertimer;
cvar_t	*cl_protocol;

cvar_t	*dbg_framesleep;
//cvar_t	*cl_snaps;

cvar_t	*cl_nolerp;

cvar_t	*cl_instantack;
cvar_t	*cl_autorecord;

cvar_t	*cl_railtrail;
cvar_t	*cl_test = &uninitialized_cvar;
cvar_t	*cl_test2;
cvar_t	*cl_test3;

cvar_t	*cl_original_dlights;
cvar_t	*cl_default_location = &uninitialized_cvar;
cvar_t	*cl_player_updates;
cvar_t	*cl_updaterate;

cvar_t	*cl_proxy;

#ifdef NO_SERVER
cvar_t	*allow_download;
cvar_t *allow_download_players;
cvar_t *allow_download_models;
cvar_t *allow_download_sounds;
cvar_t *allow_download_maps;
#endif

client_static_t	cls;
client_state_t	cl;

centity_t		cl_entities[MAX_EDICTS];

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

qboolean	send_packet_now;

extern	cvar_t *allow_download;
extern	cvar_t *allow_download_players;
extern	cvar_t *allow_download_models;
extern	cvar_t *allow_download_sounds;
extern	cvar_t *allow_download_maps;

//======================================================================

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessageFull (void)
{
	int		len, swlen;

	// the first eight bytes are just packet sequencing stuff
	len = net_message.cursize-8;
	swlen = LittleLong(len);
	if (swlen > 0)
	{
		fwrite (&swlen, 4, 1, cls.demofile);
		fwrite (net_message_buffer+8, len, 1, cls.demofile);
	}
}

void CL_WriteDemoMessage (byte *buff, int len, qboolean forceFlush)
{
	if (!cls.demorecording || cls.serverProtocol == PROTOCOL_ORIGINAL)
		return;

	if (forceFlush)
	{
		if (!cls.demowaiting)
		{
			qboolean	dropped_frame;
			int			swlen;

			dropped_frame = false;

			if (cl.demoBuff.overflowed)
			{
				Com_DPrintf ("Dropped a demo frame, maximum message size exceeded: %d > %d\n", cl.demoBuff.cursize, cl.demoBuff.maxsize);

				//we write a message regardless to keep in sync time-wise.
				SZ_Clear (&cl.demoBuff);
				SZ_WriteByte (&cl.demoBuff, svc_nop);
				dropped_frame = true;
			}
			else if (!cl.demoBuff.cursize)
			{
				Com_DPrintf ("Dropped a demo frame, no data to be written\n");
				//never write zero length frames, they cause demo end
				//we write a message regardless to keep in sync time-wise.
				SZ_Clear (&cl.demoBuff);
				SZ_WriteByte (&cl.demoBuff, svc_nop);
				dropped_frame = true;
			}

			swlen = LittleLong(cl.demoBuff.cursize);
			fwrite (&swlen, 4, 1, cls.demofile);
			fwrite (cl.demoFrame, cl.demoBuff.cursize, 1, cls.demofile);

			//fixme: this is ugly
			if (noFrameFromServerPacket == 0 && !dropped_frame)
				cl.demoLastFrame = &cl.frames[cl.frame.serverframe & UPDATE_MASK];
		}
		SZ_Clear (&cl.demoBuff);
	}

	if (len)
		SZ_Write (&cl.demoBuff, buff, len);
}

void CL_DemoDeltaEntity (const entity_state_t *from, const entity_state_t *to, qboolean force, qboolean newentity);
qboolean CL_BeginRecording (char *name)
{
	byte	buf_data[1390];
	sizebuf_t	buf;
	int		i;
	int		len;
	entity_state_t	*ent;

	FS_CreatePath (name);

	cls.demofile = fopen (name, "wb");

	if (!cls.demofile)
		return false;

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	// inform server we need to receive more data
	if (cls.serverProtocol == PROTOCOL_R1Q2)
	{
		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_RECORDING);
		MSG_WriteShort (1);
		MSG_EndWriting (&cls.netchan.message);
	}

	//
	// write out messages to hold the startup information
	//
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	//if (cls.serverProtocol == PROTOCOL_ORIGINAL)
	//	buf.maxsize = 1390;

	// send the serverdata
	MSG_BeginWriting (svc_serverdata);
	MSG_WriteLong (PROTOCOL_ORIGINAL);
	MSG_WriteLong (0x10000 + cl.servercount);
	MSG_WriteByte (1);	// demos are always attract loops
	MSG_WriteString (cl.gamedir);
	MSG_WriteShort (cl.playernum);
	MSG_WriteString (cl.configstrings[CS_NAME]);
	/*if (cls.serverProtocol == PROTOCOL_R1Q2)
	{
		MSG_WriteByte (cl.enhancedServer);
		MSG_WriteShort (MINOR_VERSION_R1Q2);
		MSG_WriteByte (cl.advancedDeltas);
	}*/
	MSG_EndWriting (&buf);

	// configstrings
	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
	{
		if (cl.configstrings[i][0])
		{
			if (buf.cursize + strlen (cl.configstrings[i]) + 64 > buf.maxsize)
			{	// write it out
				len = LittleLong (buf.cursize);
				fwrite (&len, 4, 1, cls.demofile);
				fwrite (buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			MSG_BeginWriting (svc_configstring);
			MSG_WriteShort (i);
			MSG_WriteString (cl.configstrings[i]);
			MSG_EndWriting (&buf);
		}
	}

	// baselines

	for (i=0; i<MAX_EDICTS ; i++)
	{
		ent = &cl_entities[i].baseline;
		if (!ent->modelindex)
			continue;

		if (buf.cursize + 64 > buf.maxsize)
		{	// write it out
			len = LittleLong (buf.cursize);
			fwrite (&len, 4, 1, cls.demofile);
			fwrite (buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_BeginWriting (svc_spawnbaseline);
		CL_DemoDeltaEntity (&null_entity_state, &cl_entities[i].baseline, true, true);
		MSG_EndWriting (&buf);
	}

	MSG_BeginWriting (svc_stufftext);
	MSG_WriteString ("precache\n");
	MSG_EndWriting (&buf);

	// write it to the demo file

	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	fwrite (buf.data, buf.cursize, 1, cls.demofile);

	// the rest of the demo file will be individual frames
	return true;
}

void CL_EndRecording(void)
{
	int len;

	// finish up
	len = -1;
	fwrite (&len, 4, 1, cls.demofile);
	fclose (cls.demofile);

	// inform server we are done with extra data
	if (cls.serverProtocol == PROTOCOL_R1Q2)
	{
		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_RECORDING);
		MSG_WriteShort (0);
		MSG_EndWriting (&cls.netchan.message);
	}

	cls.demofile = NULL;
	cls.demorecording = false;

	// reset delta demo state
	SZ_Clear (&cl.demoBuff);
	cl.demoLastFrame = NULL;
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	int		len;

	if (!cls.demorecording)
	{
		Com_Printf ("Not recording a demo.\n", LOG_CLIENT);
		return;
	}

	len = ftell (cls.demofile);
	
	CL_EndRecording();

	Com_Printf ("Stopped demo, recorded %d bytes.\n", LOG_CLIENT, len);
}

void CL_Ignore_f (void)
{
	ignore_t	*list, *last, *newentry;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("usage: ignore text\n", LOG_GENERAL);
		return;
	}

	list = &cl_ignores;

	last = list->next;

	newentry = Z_TagMalloc (sizeof(*list), TAGMALLOC_CLIENT_IGNORE);
	newentry->text = CopyString (Cmd_Args(), TAGMALLOC_CLIENT_IGNORE);
	newentry->next = last;
	list->next = newentry;

	Com_Printf ("%s added to ignore list.\n", LOG_GENERAL, newentry->text);
}

qboolean CL_IgnoreMatch (const char *string)
{
	ignore_t	*entry;

	entry = &cl_ignores;

	while (entry->next)
	{
		entry = entry->next;
		if (strstr (string, entry->text))
			return true;
	}

	return false;
}

void CL_Unignore_f (void)
{
	ignore_t	*entry, *last;
	char		*match;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("usage: unignore text\n", LOG_GENERAL);
		return;
	}

	match = Cmd_Args();
	entry = last = &cl_ignores;

	while (entry->next)
	{
		entry = entry->next;

		if (!Q_stricmp (match, entry->text))
		{
			last->next = entry->next;
			Z_Free (entry->text);
			Z_Free (entry);

			Com_Printf ("Ignore '%s' removed.\n", LOG_GENERAL, match);
			return;
		}
		last = entry;
	}

	Com_Printf ("Ignore '%s' not found.\n", LOG_GENERAL, match);
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
void CL_Record_f (void)
{
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("record <demoname>\n", LOG_CLIENT);
		return;
	}

	if (cls.demorecording)
	{
		Com_Printf ("Already recording.\n", LOG_CLIENT);
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf ("You must be in a level to record.\n", LOG_CLIENT);
		return;
	}

	if (cl.attractloop)
	{
		Com_Printf ("Unable to record from a demo stream due to insufficient deltas.\n", LOG_CLIENT);
		return;
	}

	if (strstr (Cmd_Argv(1), "..") || strchr (Cmd_Argv(1), '/') || strchr (Cmd_Argv(1), '\\') )
	{
		Com_Printf ("Illegal filename.\n", LOG_CLIENT);
		return;
	}

	//
	// open the demo file
	//
	Com_sprintf (name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	FS_CreatePath (name);

	if (!CL_BeginRecording (name))
	{
		Com_Printf ("ERROR: Couldn't open %s for writing.\n", LOG_CLIENT, name);
	}
	else
	{
		/*if (cls.serverProtocol == PROTOCOL_R1Q2)
		{
			//make it a bit more obvious for the bleeding idiots out there
			//Com_Printf ("WARNING: Demos recorded at cl_protocol 35 are not guaranteed to replay properly!\n", LOG_CLIENT);
			Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n", LOG_CLIENT);
			S_StartLocalSound ("player/burn1.wav");
			S_StartLocalSound ("player/burn2.wav");
			con.ormask = 128;
			Com_Printf ("Demos recorded at cl_protocol 35 are\nnot guaranteed to replay properly!\n", LOG_CLIENT);
			con.ormask = 0;
			Com_Printf("\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n", LOG_CLIENT);
		}*/
		Com_Printf ("recording to %s.\n", LOG_CLIENT, name);
	}
}

//======================================================================

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Com_Printf ("Unknown command \"%s\"\n", LOG_CLIENT, cmd);
		return;
	}

	Com_DPrintf ("Cmd_ForwardToServer: forwarding '%s'\n", cmd);

	//Com_Printf ("fwd: %s %s\n", cmd, Cmd_Args());

	MSG_BeginWriting (clc_stringcmd);
	MSG_Print (cmd);
	if (Cmd_Argc() > 1)
	{
		MSG_Print (" ");
		MSG_Print (Cmd_Args());
	}
	MSG_EndWriting (&cls.netchan.message);
	send_packet_now = true;
}

/*void CL_Setenv_f( void )
{
	int argc = Cmd_Argc();

	if ( argc > 2 )
	{
		char buffer[1000];
		int i;

		strcpy( buffer, Cmd_Argv(1) );
		strcat( buffer, "=" );

		for ( i = 2; i < argc; i++ )
		{
			strcat( buffer, Cmd_Argv( i ) );
			strcat( buffer, " " );
		}

		putenv( buffer );
	}
	else if ( argc == 2 )
	{
		char *env = getenv( Cmd_Argv(1) );

		if ( env )
		{
			Com_Printf( "%s=%s\n", Cmd_Argv(1), env );
		}
		else
		{
			Com_Printf( "%s undefined\n", Cmd_Argv(1), env );
		}
	}
}*/

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	//if (cls.state != ca_connected && cls.state != ca_active)
	if (cls.state < ca_connected)
	{
		Com_Printf ("Can't \"%s\", not connected\n", LOG_CLIENT, Cmd_Argv(0));
		return;
	}

	//r1: support \xxx escape sequences for testing various evil hacks
#ifdef _DEBUG
	{
		char *rew = Cmd_Args();
		char args[MAX_STRING_CHARS];
		char tmp[4];
		memset (args, 0, sizeof(args));
		strcpy (args, rew);
		memset (rew, 0, sizeof(args));
		strcpy (rew, args);
		while (*rew && *rew+3) {
			if (*rew == '\\' && isdigit(*(rew+1)) && isdigit(*(rew+2)) && isdigit(*(rew+3))) {
				Q_strncpy (tmp, rew+1, 3);
				*rew = atoi(tmp);
				memcpy (rew+1, rew+4, 1024 - (rew - Cmd_Args() + 3));
				//*(rew + (rew - Cmd_Args() - 2)) = 0;
			}
			rew++;
		}
	}
#endif

	//Com_Printf ("fwd: %s\n", Cmd_Args());

	// don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		Com_DPrintf ("CL_ForwardToServer_f: Wrote '%s'\n", Cmd_Args());

		MSG_WriteByte (clc_stringcmd);
		MSG_Print (Cmd_Args());
		MSG_EndWriting (&cls.netchan.message);

		send_packet_now = true;
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	// never pause in multiplayer
	if (!cl.attractloop && (Cvar_IntValue ("maxclients") > 1 || !Com_ServerState ()))
	{
		Cvar_Set ("paused", "0");
		return;
	}

	Cvar_SetValue ("paused", (float)!cl_paused->intvalue);
}

/*
==================
CL_Quit_f
==================
*/
NORETURN void CL_Quit_f (void)
{
	CL_Disconnect (false);
	Com_Quit ();
}

/*
================
CL_Drop

Called after an ERR_DROP was thrown
================
*/
void CL_Drop (qboolean skipdisconnect, qboolean nonerror)
{
	if (cls.state == ca_uninitialized)
		return;

	if (cls.state == ca_disconnected)
		return;

	CL_Disconnect (skipdisconnect);

	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque

	//reset if we crashed a menu
	M_ForceMenuOff ();

	if (!nonerror)
		cls.serverProtocol = 0;
}


/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (int useProtocol)
{
	netadr_t	adr;
	int			port;
	unsigned	msglen;

#ifdef CLIENT_DLL
	if (!cllib_active) {
		//retry loading it now
		CL_ClDLL_Restart_f ();
		//if (!cllib_active)
			//Com_Error (ERR_DROP, "no client_dll.dll loaded for current game.");
	}
#endif

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address: %s\n", LOG_CLIENT, cls.servername);
		cls.connect_time = 0;
		return;
	}

	if (adr.port == 0)
		adr.port = ShortSwap (PORT_SERVER);

	if (qport->intvalue == -1 || cls.proxyState == ps_active)
		port = (int)(random() * 0xFFFF);
	else
		port = qport->intvalue;

	userinfo_modified = false;

	if (Com_ServerState() == ss_demo)
	{
		msglen = MAX_USABLEMSG;
		cls.serverProtocol = 35;
	}
	else
	{
		msglen = Cvar_IntValue ("net_maxmsglen");

		if (!cls.serverProtocol)
		{
			if (cl_protocol->intvalue)
				cls.serverProtocol = cl_protocol->intvalue;
			else if (useProtocol)
				cls.serverProtocol = useProtocol;
			else
				cls.serverProtocol = PROTOCOL_R1Q2;
		}
	}

	if (cls.serverProtocol == PROTOCOL_R1Q2)
		port &= 0xFF;

	cls.quakePort = port;

	Com_DPrintf ("Cl_SendConnectPacket: protocol=%d, port=%d, challenge=%u\n", cls.serverProtocol, port, cls.challenge);

	//r1: only send enhanced connect string on new protocol in order to avoid confusing
	//    other engine mods which may or may not extend this.


	if (cls.serverProtocol == PROTOCOL_R1Q2)
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "connect %i %i %i \"%s\" %u %u\n", cls.serverProtocol, port, cls.challenge, Cvar_Userinfo(), msglen, MINOR_VERSION_R1Q2);
	else
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "connect %i %i %i \"%s\"\n", cls.serverProtocol, port, cls.challenge, Cvar_Userinfo());
}

/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	S_StopAllSounds ();

	if (cls.state == ca_connected)
	{
		Com_Printf ("reconnecting (soft)...\n", LOG_CLIENT);
		//cls.state = ca_connected;
		MSG_WriteByte (clc_stringcmd);
		MSG_WriteString ("new");		
		MSG_EndWriting (&cls.netchan.message);
		send_packet_now = true;
		return;
	}

	if (cls.servername[0])
	{
		if (cls.state >= ca_connected) {
			Com_Printf ("disconnecting\n", LOG_CLIENT);
			strcpy (cls.lastservername, cls.servername);
			CL_Disconnect(false);
			cls.connect_time = cls.realtime - 1500;
		} else
			cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		//Com_Printf ("reconnecting (hard)...\n");
	}
	
	if (cls.lastservername[0])
	{
		cls.connect_time = -99999;
		cls.state = ca_connecting;
		Com_Printf ("reconnecting (hard)...\n", LOG_CLIENT);
		strcpy (cls.servername, cls.lastservername);
	}
	else
	{
		Com_Printf ("No server to reconnect to.\n", LOG_CLIENT);
	}
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Com_ServerState() )
	{
		cls.state = ca_connecting;
		strcpy (cls.servername, "localhost");
		// we don't need a challenge on the localhost
		CL_SendConnectPacket (0);
		return;
//		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (cl_proxy->string[0] && cls.proxyState != ps_active)
	{
		if (!NET_StringToAdr (cl_proxy->string, &cls.proxyAddr))
		{
			Com_Printf ("Bad proxy address: %s\n", LOG_CLIENT, cl_proxy->string);
			cls.state = ca_disconnected;
			cls.proxyState = ps_none;
			return;
		}
	
		if (cls.proxyAddr.port == 0)
			cls.proxyAddr.port = ShortSwap (27999);

		cls.proxyState = ps_pending;
	}
	else
	{
		if (!NET_StringToAdr (cls.servername, &adr))
		{
			Com_Printf ("Bad server address: %s\n", LOG_CLIENT, cls.servername);
			cls.state = ca_disconnected;
			cls.proxyState = ps_none;
			return;
		}
	
		if (adr.port == 0)
			adr.port = ShortSwap (PORT_SERVER);
	}

	cls.connect_time = cls.realtime;	// for retransmit requests

	//_asm int 3;
	if (cls.proxyState == ps_pending)
	{
		Com_Printf ("Connecting to %s...\n", LOG_CLIENT, NET_AdrToString(&cls.proxyAddr));
		Netchan_OutOfBandProxyPrint (NS_CLIENT, &cls.proxyAddr, "proxygetchallenge\n");
	}
	else
	{
		Com_Printf ("Connecting to %s...\n", LOG_CLIENT, cls.servername);
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "getchallenge\n");
	}
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	char		*server, *p;
	netadr_t	adr;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: connect <server>\n", LOG_GENERAL);
		return;	
	}
	
	if (Com_ServerState ())
	{	// if running a local server, kill it and reissue
#ifndef NO_SERVER
		SV_Shutdown ("Server has shut down", false, false);
#endif
	}
	else
	{
		//we do this anyway below
		//CL_Disconnect (false);
	}

	server = Cmd_Argv (1);

	// quake2://protocol support
	if (!strncmp (server, "quake2://", 9))
		server += 9;

	p = strchr (server, '/');
	if (p)
		p[0] = 0;

	NET_Config (NET_CLIENT);		// allow remote

	if (!NET_StringToAdr (server, &adr))
	{
		Com_Printf ("Bad server address: %s\n", LOG_CLIENT, server);
		return;
	}

	if (adr.port == 0)
		adr.port = ShortSwap (PORT_SERVER);

	CL_Disconnect (false);

	//reset protocol attempt if we're connecting to a different server
	if (!NET_CompareAdr (&adr, &cls.netchan.remote_address))
	{
		Com_DPrintf ("Resetting protocol attempt since %s is not ", NET_AdrToString (&adr));
		Com_DPrintf ("%s.\n", NET_AdrToString (&cls.netchan.remote_address));
		cls.serverProtocol = 0;
	}

	cls.state = ca_connecting;
	Q_strncpy (cls.servername, server, sizeof(cls.servername)-1);
	strcpy (cls.lastservername, cls.servername);

	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char		message[1024];
	netadr_t	to;

	//r1: buffer check ffs!
	if ((strlen(Cmd_Args()) + strlen(rcon_client_password->string) + 16) >= sizeof(message)) {
		Com_Printf ("Length of password + command exceeds maximum allowed length.\n", LOG_CLIENT);
		return;
	}

	*(int *)message = -1;
	message[4] = 0;

	NET_Config (NET_CLIENT);		// allow remote

	strcat (message, "rcon ");

	if (rcon_client_password->string[0])
	{
		strcat (message, rcon_client_password->string);
		strcat (message, " ");
	}

	strcat (message, Cmd_Args());

	/*for (i=1 ; i<Cmd_Argc() ; i++)
	{
		strcat (message, Cmd_Argv(i));
		strcat (message, " ");
	}*/
	

	if (cls.state >= ca_connected)
	{
		to = cls.netchan.remote_address;
	}
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n", LOG_CLIENT);

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = ShortSwap (PORT_SERVER);
	}
	
	//allow remote response
	incoming_allowed[incoming_allowed_index & 15].remote = to;
	incoming_allowed[incoming_allowed_index & 15].type = CL_RCON;
	incoming_allowed[incoming_allowed_index & 15].time = cls.realtime + 2500;
	incoming_allowed_index++;

	NET_SendPacket (NS_CLIENT, (int)strlen(message)+1, message, &to);
}

void CL_ServerStatus_f (void)
{
	netadr_t	adr;
	int			i;
	int			type;
	
	i = 1;

	if (!strcmp (Cmd_Argv(1), "/s"))
	{
		i++;
		type = CL_SERVER_STATUS_FULL;
	}
	else if (!strcmp (Cmd_Argv(1), "/i"))
	{
		i++;
		type = CL_SERVER_INFO;
	}
	else
	{
		type = CL_SERVER_STATUS;
	}

	NET_Config (NET_CLIENT);		// allow remote

	if (i >= Cmd_Argc())
	{
		if (cls.state < ca_connected)
		{
			Com_Printf ("usage: serverstatus [/s|/i] [server]\n", LOG_GENERAL);
			return;
		}
		adr = cls.netchan.remote_address;
	}
	else
	{
		if (!NET_StringToAdr (Cmd_Argv(i), &adr))
		{
			Com_Printf ("Bad address %s\n", LOG_CLIENT, Cmd_Argv(i));
			return;
		}
	}

	if (!adr.port)
		adr.port = ShortSwap (PORT_SERVER);

	if (type == CL_SERVER_INFO)
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "info 34");
	else
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "status");

	incoming_allowed[incoming_allowed_index & 15].remote = adr;
	incoming_allowed[incoming_allowed_index & 15].type = type;
	incoming_allowed[incoming_allowed_index & 15].time = cls.realtime + 2500;
	incoming_allowed_index++;
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	cvar_t	*gameDirHack;

	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (&cl_entities, 0, sizeof(cl_entities));

	//r1 unprotect game
	gameDirHack = Cvar_FindVar ("game");
	gameDirHack->flags &= ~CVAR_NOSET;

	//r1: local ents clear
	Le_Reset ();

	//r1: reset
	cl.maxclients = MAX_CLIENTS;
	SZ_Clear (&cls.netchan.message);

	SZ_Init (&cl.demoBuff, cl.demoFrame, sizeof(cl.demoFrame));
	cl.demoBuff.allowoverflow = true;

	cl.settings[SVSET_FPS] = 10;
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (qboolean skipdisconnect)
{
	byte	final[16];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo->intvalue)
	{
		unsigned int	time;
		
		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.2f seconds: %3.1f fps\n", LOG_CLIENT, cl.timedemo_frames,
			time/1000.0f, cl.timedemo_frames*1000.0f / time);
	}

	VectorClear (cl.refdef.blend);
	re.CinematicSetPalette(NULL);

	//r1: why?
	//M_ForceMenuOff ();

	cls.connect_time = 0;

#ifdef CINEMATICS
	SCR_StopCinematic ();
#endif

	if (cls.demorecording)
		CL_EndRecording();
		//CL_Stop_f ();

	// send a disconnect message to the server
	if (!skipdisconnect)
	{
		final[0] = clc_stringcmd;
		strcpy ((char *)final+1, "disconnect");

		Netchan_Transmit (&cls.netchan, 11, final);
		Netchan_Transmit (&cls.netchan, 11, final);
		Netchan_Transmit (&cls.netchan, 11, final);
	}

	CL_ClearState ();

	// stop download
	if (cls.download)
	{
		fclose(cls.download);
		cls.download = NULL;
	}

#ifdef USE_CURL
	CL_CancelHTTPDownloads (true);
	cls.downloadReferer[0] = 0;
#endif

	cls.downloadname[0] = 0;
	cls.downloadposition = 0;

	cls.state = ca_disconnected;
	cls.proxyState = ps_none;
	cls.servername[0] = 0;

	Cvar_ForceSet ("$mapname", "");
	Cvar_ForceSet ("$game", "");
	Cvar_ForceSet ("$serverip", "");

	//r1: swap games if needed
	Cvar_GetLatchedVars();

	NET_SetProxy (NULL);
	MSG_Clear();
}

void CL_Disconnect_f (void)
{
	if (cls.state != ca_disconnected)
	{
		cls.serverProtocol = 0;
		cls.key_dest = key_console;
		Com_Error (ERR_HARD, "Disconnected from server");
	}
}

//Spam repeated cmd for overflow checkings
#ifdef _DEBUG
void CL_Spam_f (void)
{
	char	cmdBuff[1380] = {0};
	int		count;
	int		i;
	char	*what;

	count = atoi (Cmd_Argv(2));

	if (!count)
		return;

	what = Cmd_Argv(1);

	Q_strncpy (cmdBuff, Cmd_Argv(3), sizeof(cmdBuff)-1);
	if (cmdBuff[0])
		strcat (cmdBuff, " ");

	if (count * strlen(what) >= sizeof(cmdBuff)-strlen(cmdBuff)-2)
	{
		Com_Printf ("Too long an expanded string.\n", LOG_CLIENT);
		return;
	}

	for (i = 0; i < count; i++)
		strcat (cmdBuff, what);

	strcat (cmdBuff, "\n");

	MSG_WriteByte (clc_stringcmd);
	MSG_WriteString (cmdBuff);
	MSG_EndWriting (&cls.netchan.message);

	Com_Printf ("Wrote %d bytes stringcmd.\n", LOG_CLIENT, count * strlen(what));
}
#endif

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
#ifdef _DEBUG
void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() < 3)
	{
		Com_Printf ("packet <destination> <contents>\n", LOG_CLIENT);
		return;
	}

	NET_Config (NET_CLIENT);		// allow remote

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Com_Printf ("Bad address\n", LOG_CLIENT);
		return;
	}

	if (!adr.port)
		adr.port = ShortSwap (PORT_SERVER);

	in = Cmd_Args2(2);
	out = send+4;

	*(int *)send = -1;

	l = (int)strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (out - send >= sizeof(send)-1)
			break;

		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	incoming_allowed[incoming_allowed_index & 15].remote = adr;
	incoming_allowed[incoming_allowed_index & 15].type = CL_UNDEF;
	incoming_allowed[incoming_allowed_index & 15].time = cls.realtime + 2500;
	incoming_allowed_index++;

	NET_SendPacket (NS_CLIENT, (int)(out-send), send, &adr);
}
#endif

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	char	*cmd;

	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	if (cls.demofile)
		CL_EndRecording();

	//force screen update in case user has screenshot etc they want doing
	SCR_UpdateScreen ();

	cmd = Cmd_MacroExpandString("$cl_endmapcmd");
	if (cmd)
	{
		if (cmd[0])
		{
			char	*p;
			//note, can't use Cmd_ExecuteString as it doesn't handle ;
			//but we must otherwise we don't run immediately!
			p = cmd;

			for (;;)
			{
				p = strchr (p, ';');
				if (p)
				{
					p[0] = 0;
					Cmd_ExecuteString (cmd);
					p = cmd = p + 1;
					if (!cmd[0])
						break;
				}
				else
				{
					Cmd_ExecuteString (cmd);
					break;
				}
			}
		}
	}
	else
		Com_Printf ("WARNING: Error expanding $cl_endmapcmd, ignored.\n", LOG_CLIENT|LOG_WARNING);

	//r1: prevent manual issuing crashing game
	if (cls.state < ca_connected)
		return;

	//r1: stop loading models right now
	deferred_model_index = MAX_MODELS;

	SCR_BeginLoadingPlaque ();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Com_Printf ("\nChanging map...\n", LOG_CLIENT);

	//shut up
	noFrameFromServerPacket = 0;
}


/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
void CL_ParseStatusMessage (void)
{
	char	*s;

	s = MSG_ReadString (&net_message);

	Com_Printf ("%s\n", LOG_CLIENT, s);
	M_AddToServerList (net_from, s);
}

//FIXME: add this someday
/*void CL_GetServers_f (void)
{
	netadr_t		adr;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Usage: getservers <master>\n", LOG_GENERAL);
		return;
	}

	NET_StringToAdr (Cmd_Argv(1), &adr);

	Netchan_OutOfBandPrint (NS_CLIENT, &adr, "getservers");

	//allow remote response
	incoming_allowed[incoming_allowed_index & 15].remote = to;
	incoming_allowed[incoming_allowed_index & 15].type = CL_MASTER_QUERY;
	incoming_allowed[incoming_allowed_index & 15].time = cls.realtime + 2500;
	incoming_allowed_index++;
}*/

/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f (void)
{
	int				i;
	netadr_t		adr;
	char			name[16];
	const char		*adrstring;
	//cvar_t		*noudp;
	//cvar_t		*noipx;

	NET_Config (NET_CLIENT);		// allow remote

	// send a broadcast packet
	Com_Printf ("pinging broadcast...\n", LOG_CLIENT|LOG_NOTICE);

	adr.type = NA_BROADCAST;
	adr.port = ShortSwap(PORT_SERVER);

	//r1: only ping original, r1q2 servers respond to either, but 3.20 server would
	//reply with errors on receiving enhanced info
	Netchan_OutOfBandPrint (NS_CLIENT, &adr, "info 34\n");

	//also ping local server
	adr.type = NA_IP;;
	*(int *)&adr.ip = 0x100007F;
	adr.port = ShortSwap(PORT_SERVER);
	Netchan_OutOfBandPrint (NS_CLIENT, &adr, "info 34\n");

	// send a packet to each address book entry
	for (i=0 ; i < 32; i++)
	{
		Com_sprintf (name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString (name);
		if (!adrstring || !adrstring[0])
			continue;

		Com_Printf ("pinging %s...\n", LOG_CLIENT|LOG_NOTICE, adrstring);
		if (!NET_StringToAdr (adrstring, &adr))
		{
			Com_Printf ("Bad address: %s\n", LOG_CLIENT, adrstring);
			continue;
		}
		if (!adr.port)
			adr.port = ShortSwap(PORT_SERVER);
		//Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_R1Q2));
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "info %i\n", PROTOCOL_ORIGINAL);
	}
}


/*
=================
CL_Skins_f

Load or download any custom player skins and models
=================
*/

void CL_Skins_f (void)
{
	int		i;

	for (i=0 ; i < MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		//if (developer->intvalue)
		Com_Printf ("client %i: %s\n", LOG_CLIENT, i, cl.configstrings[CS_PLAYERSKINS+i]); 
		//SCR_UpdateScreen ();
		//Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
	}
	//Com_Printf ("Precached all skins.\n", LOG_CLIENT);
}

void CL_ProxyPacket (void)
{
	char	*s;
	char	*c;
	
	if (cls.proxyState != ps_pending)
		return;

	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message);	// skip the -2

	s = MSG_ReadStringLine (&net_message);

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv(0);

	if (!strcmp (c, "proxychallenge"))
	{
		netadr_t	remote_addr;
		unsigned long challenge = strtoul(Cmd_Argv(1), NULL, 10);

		if (!NET_StringToAdr (cls.servername, &remote_addr))
			return;

		if (remote_addr.port == 0)
			remote_addr.port = ShortSwap (PORT_SERVER);

		Netchan_OutOfBandProxyPrint (NS_CLIENT, &cls.proxyAddr, "proxyconnect %lu %s\n", challenge, NET_AdrToString (&remote_addr));
	}
	else if (!strcmp (c, "proxyactive"))
	{
		int	port;
		
		port = atoi (Cmd_Argv(1));

		if (port >= 1024)
			net_from.port = ShortSwap(port);

		NET_SetProxy (&net_from);
		cls.proxyAddr = net_from;
		cls.proxyState = ps_active;
		cls.connect_time = -99999; // fire immediately
	}
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	int		type;
	int		i;

	char	*s;
	char	*c;

	netadr_t	remote_addr;
	netadr_t	*remote;

	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message);	// skip the -1

	s = MSG_ReadStringLine (&net_message);

	NET_StringToAdr (cls.servername, &remote_addr);

	remote = &remote_addr;

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv(0);

	type = CL_UNDEF;

	//r1: server responding to a status broadcast (ignores security check due
	//to broadcasts responding)
	if (!strcmp(c, "info"))
	{
		if (cls.state == ca_disconnected || cls.key_dest != key_game)
			CL_ParseStatusMessage ();
		return;
	}
	else if (!strcmp (c, "passive_connect"))
	{
		if (!cls.passivemode)
		{
			Com_DPrintf ("Illegal %s from %s.  Ignored.\n", c, NET_AdrToString (&net_from));
		}
		else
		{
			Com_Printf ("Received %s from %s -- beginning connection.\n", LOG_CLIENT, c, NET_AdrToString (&net_from));
			strcpy (cls.servername, NET_AdrToString (&net_from));
			cls.state = ca_connecting;
			cls.connect_time = -99999;
			cls.passivemode = false;
		}
		return;
	}

	//r1: security check. (only allow from current connected server
	//and last destinations client sent a packet to)
	for (i = 0; i < sizeof(incoming_allowed) / sizeof(incoming_allowed[0]); i++)
	{
		if (incoming_allowed[i].time > cls.realtime && NET_CompareBaseAdr (&net_from, &incoming_allowed[i].remote))
		{
			type = incoming_allowed[i].type;
			//invalidate
			incoming_allowed[i].time = 0;
			goto safe;
		}
	}

	if (!NET_CompareBaseAdr (&net_from, remote) && !NET_CompareBaseAdr (&net_from, &cls.proxyAddr))
	{
		Com_DPrintf ("Illegal %s from %s.  Ignored.\n", c, NET_AdrToString (&net_from));
		return;
	}

safe:

	if (type == CL_UNDEF)
	{
		//r1: spamfix: don't echo packet type mid-game
		if (cls.state < ca_active)
			Com_Printf ("%s: %s\n", LOG_CLIENT, NET_AdrToString (&net_from), c);
	}

	// server connection
	if (!strcmp(c, "client_connect"))
	{
#ifdef ANTICHEAT
		qboolean	try_to_use_anticheat;
#endif
		int			i;
		char		*buff, *p;

		if (cls.state == ca_connected)
		{
			Com_DPrintf ("Dup connect received.  Ignored.\n");
			return;

			//r1: note, next 2 should be redundant with the above global check.
		} else if (cls.state == ca_disconnected) {
			//FIXME: this should never happen (disconnecting nukes remote, no remote
			//= no packet)
			Com_DPrintf ("Received connect when disconnected.  Ignored.\n");
			return;
		} else if (cls.state == ca_active) {
			Com_DPrintf ("Illegal connect when already connected !! (q2msgs?).  Ignored.\n");
			return;
		}

		Com_DPrintf ("client_connect: new\n");

		Netchan_Setup (NS_CLIENT, &cls.netchan, &net_from, cls.serverProtocol, cls.quakePort, 0);

		buff = NET_AdrToString(&cls.netchan.remote_address);

#ifdef ANTICHEAT
		try_to_use_anticheat = false;
#endif

		for (i = 1; i < Cmd_Argc(); i++)
		{
			p = Cmd_Argv(i);
			if (!strncmp (p, "dlserver=", 9))
			{
#ifdef USE_CURL
				p += 9;
				Com_sprintf (cls.downloadReferer, sizeof(cls.downloadReferer), "quake2://%s", buff);
				CL_SetHTTPServer (p);
				if (cls.downloadServer[0])
					Com_Printf ("HTTP downloading enabled, URL: %s\n", LOG_CLIENT|LOG_NOTICE, cls.downloadServer);
#else
				Com_Printf ("HTTP downloading supported by server but this client was built without USE_CURL, bad luck.\n", LOG_CLIENT);
#endif
			}
#ifdef ANTICHEAT
			else if (!strncmp (p, "ac=", 3))
			{
				p+= 3;
				if (!p[0])
					continue;
				if (atoi (p))
					try_to_use_anticheat = true;
			}
#endif
		}

		//note, not inside the loop as we could potentially clobber cmd_argc/cmd_argv
#ifdef ANTICHEAT
		if (try_to_use_anticheat)
		{
			MSG_WriteByte (clc_nop);
			MSG_EndWriting (&cls.netchan.message);
			Netchan_Transmit (&cls.netchan, 0, NULL);
			S_StopAllSounds ();
			con.ormask = 128;
			Com_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n", LOG_CLIENT);
			Com_Printf ("Loading anticheat, this may take a few moments...\n", LOG_GENERAL);
			Com_Printf("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n", LOG_CLIENT);
			con.ormask = 0;
			SCR_UpdateScreen ();
			SCR_UpdateScreen ();
			SCR_UpdateScreen ();
			if (!Sys_GetAntiCheatAPI ())
				Com_Printf ("        ERROR: anticheat.dll failed to load\n"
							"Either the file is missing, or something is preventing\n"
							"it from loading or connecting to the anticheat server.\n"
							"This is commonly caused by over-aggressive anti-virus\n"
							"software. Try disabling any anti-virus or other security\n"
							"software or add an exception for anticheat.dll. Make sure\n"
							"your system date/time is set correctly as this can prevent\n"
							"anticheat from working.\n"
							"\n"
							"Trying to connect without anticheat support.\n", LOG_GENERAL);
		}
#endif
		p = strchr (buff, ':');
		if (p)
			p[0] = 0;
		Cvar_ForceSet ("$serverip", buff);

		MSG_WriteByte (clc_stringcmd);
		MSG_WriteString ("new");
		MSG_EndWriting (&cls.netchan.message);

		cls.key_dest = key_game;

		//CL_FixCvarCheats();
		send_packet_now = true;
		cls.state = ca_connected;
		return;
	}

	// remote command from gui front end
	// r1: removed, this is dangerous to leave in with new NET_IsLocalAddress code.
	/*if (!strcmp(c, "cmd"))
	{
		if (!NET_IsLocalAddress(&net_from))
		{
			Com_DPrintf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		Sys_AppActivate ();
		s = MSG_ReadString (&net_message);
		Cbuf_AddText (s);
		Cbuf_AddText ("\n");
		return;
	}*/

	// print command from somewhere
	if (!strcmp(c, "print"))
	{
		s = MSG_ReadString (&net_message);

		switch (type)
		{
			case CL_RCON:
				//rcon can come in multiple packets
				incoming_allowed[incoming_allowed_index & 15].remote = net_from;
				incoming_allowed[incoming_allowed_index & 15].type = CL_RCON;
				incoming_allowed[incoming_allowed_index & 15].time = cls.realtime + 2500;
				incoming_allowed_index++;

				//intentional fallthrough
			case CL_SERVER_INFO:
				Com_Printf ("%s", LOG_CLIENT, s);
				break;
			case CL_SERVER_STATUS:
			case CL_SERVER_STATUS_FULL:
				{
					char	*p;
					char	*player_name;
					char	*player_score;
					char	*player_ping;

					//skip the serverinfo
					if (type == CL_SERVER_STATUS)
					{
						p = strstr (s, "\n");
						if (!p)
							return;
						else
							p++;
					}
					else
					{
						int		flip;

						Com_Printf ("Server Info\n"
									"-----------\n", LOG_CLIENT);

						flip = 0;
						p = s + 1;

						//make it more readable
						while (p[0])
						{
							if (p[0] == '\\')
							{
								if (flip)
									p[0] = '\n';
								else
									p[0] = '=';
								flip ^= 1;
							}
							else if (p[0] == '\n')
							{
								while (p[0] && p[0] == '\n')
								{
									p[0] = 0;
									p++;
								}
								break;
							}
							p++;
						}
						Com_Printf ("%s\n", LOG_CLIENT, s+1);
					}

					Com_Printf ("Name            Score Ping\n"
								"--------------- ----- ----\n", LOG_CLIENT);

		/*
		0 0 "OCLD"
		86 86 "a vehicle"
		81 28 "1ST-Timer"
		27 51 "[DSH]Fella"
		86 24 "[MCB]Jonny"
		*/
					//icky icky parser
					while (p[0])
					{
						if (isdigit (p[0]))
						{
							player_score = p;
							p = strchr (player_score, ' ');
							if (p)
							{
								p[0] = 0;
								p++;
								if (isdigit (p[0]))
								{
									player_ping = p;
									p = strchr (player_ping, ' ');
									if (p)
									{
										p[0] = 0;
										p++;
										if (p[0] == '"')
										{
											player_name = p + 1;
											p = strchr (player_name, '"');
											if (p)
											{
												p[0] = 0;
												p++;
												if (p[0] == '\n')
												{
													Com_Printf ("%-15s %5s %4s\n", LOG_CLIENT, player_name, player_score, player_ping);
													p++;
												} else return;
											} else return;
										} else return;
									} else return;
								} else return;
							} else return;
						} else return;
					}
				}
				break;
			default:
				//BIG HACK to allow new client on old server!

				//r1: spoofed spam fix, only allow print during connection process, other legit cases should be handled above
				if (cls.state == ca_connecting || cls.state == ca_connected)
					Com_Printf ("%s", LOG_CLIENT, s);

				//r1: security fix: only do protocol fallback when connecting.
				if (cls.state == ca_connecting)
				{
					if (!strstr (s, "full") &&
						!strstr (s, "locked") &&
						!strncmp (s, "Server is ", 10) &&
						cls.serverProtocol != PROTOCOL_ORIGINAL)
					{
						Com_Printf ("Retrying with protocol %d.\n", LOG_CLIENT, PROTOCOL_ORIGINAL);
						cls.serverProtocol = PROTOCOL_ORIGINAL;
						//force immediate retry
						cls.connect_time = -99999;

					}
				}
				break;
		}
		return;
	}

	// ping from somewhere

	//r1: removed, could be used for flooding modemers
	//r1: tested, can even lag out a cable connect !

	/*if (!strcmp(c, "ping"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "ack");
		return;
	}*/

	// challenge from the server we are connecting to
	if (!strcmp(c, "challenge"))
	{
		int		protocol = PROTOCOL_ORIGINAL;
		int		i;
		char	*p;

		if (cls.state != ca_connecting)
		{
			Com_DPrintf ("Illegal challenge from server whilst already connected, ignored.\n");
			return;
		}

		cls.challenge = atoi(Cmd_Argv(1));

		// available protocol versions now in challenge, until few months we still default to brute.
		for (i = 2; i < Cmd_Argc(); i++)
		{
			p = Cmd_Argv(i);
			if (!strncmp (p, "p=", 2))
			{
				p += 2;
				if (!p[0])
					continue;
				for (;;)
				{
					i = atoi (p);
					if (i == PROTOCOL_R1Q2)
						protocol = i;
					p = strchr(p, ',');
					if (!p)
						break;
					p++;
					if (!p[0])
						break;
				}
				break;
			}
		}

		//r1: reset the timer so we don't send dup. getchallenges
		cls.connect_time = cls.realtime;
		CL_SendConnectPacket (protocol);
		return;
	}

	// echo request from server
	//r1: removed, no real purpose and possible attack vector
	/*if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, &net_from, "%s", Cmd_Argv(1) );
		return;
	}*/

	Com_DPrintf ("Unknown connectionless packet command %s\n", c);
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	int i;

#ifdef _DEBUG
	memset (net_message_buffer, 31, sizeof(net_message_buffer));
#endif

	while ((i = (NET_GetPacket (NS_CLIENT, &net_from, &net_message))))
	{
		//
		// remote command packet
		//
		if (*(int *)net_message_buffer == -1)
		{
			if (i == -1 && cls.key_dest != key_game)
				Com_Printf ("Port unreachable from %s\n", LOG_CLIENT|LOG_NOTICE, NET_AdrToString (&net_from));
			else
				CL_ConnectionlessPacket ();
			continue;
		}
		else if (*(int *)net_message_buffer == -2)
		{
			CL_ProxyPacket ();
			continue;
		}

		if (cls.state <= ca_connecting)
			continue;		// dump it if not connected

		if (net_message.cursize < 8)
		{
			//r1: delegated to DPrintf (someone could spam your console with crap otherwise)
			Com_DPrintf ("%s: Runt packet\n",NET_AdrToString(&net_from));
			continue;
		}

		//
		// packet from server
		//
		if (!NET_CompareAdr (&net_from, &cls.netchan.remote_address))
		{
			Com_DPrintf ("%s:sequenced packet without connection\n"
				,NET_AdrToString(&net_from));
			continue;
		}

		if (i == -1)
			Com_Error (ERR_HARD, "Connection reset by peer.");

		if (!Netchan_Process(&cls.netchan, &net_message))
			continue;		// wasn't accepted for some reason

		if (cls.netchan.got_reliable)
		{
			if (cls.state == ca_connected)
				send_packet_now = true;
			else if (cl_instantack->intvalue)
				Netchan_Transmit (&cls.netchan, 0, NULL);

			if (cl_shownet->intvalue == 1)
				Com_Printf ("r", LOG_CLIENT);
		}

		if (CL_ParseServerMessage ())
		{
			CL_AddNetgraph ();

			if (scr_sizegraph->intvalue)
				CL_AddSizegraph ();
		}

		//
		// we don't know if it is ok to save a demo message until
		// after we have parsed the frame
		//
		if (cls.demorecording && !cls.demowaiting && cls.serverProtocol == PROTOCOL_ORIGINAL)
			CL_WriteDemoMessageFull ();
	}

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && cls.realtime - cls.netchan.last_received > cl_timeout->intvalue*1000)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf ("\nServer connection timed out.\n", LOG_CLIENT);
#ifdef _DEBUG
			CL_Reconnect_f ();
#else
			CL_Disconnect (false);
#endif
			return;
		}
	}
	else
		cl.timeoutcount = 0;
	
}


//=============================================================================

/*
==============
CL_FixUpGender_f
==============
*/

//r1: FIXME: this seems.. wrong. yet another excuse for a client dll.
void CL_FixUpGender(void)
{
	char *p;
	char sk[80];

	if (gender_auto->intvalue) {

		if (gender->modified) {
			// was set directly, don't override the user
			gender->modified = false;
			return;
		}

		Q_strncpy(sk, skin->string, sizeof(sk) - 1);
		if ((p = strchr(sk, '/')) != NULL)
			p[0] = 0;
		if (Q_stricmp(sk, "male") == 0 || Q_stricmp(sk, "cyborg") == 0)
			Cvar_Set ("gender", "male");
		else if (Q_stricmp(sk, "female") == 0 || Q_stricmp(sk, "crackhor") == 0)
			Cvar_Set ("gender", "female");
		else
			Cvar_Set ("gender", "none");
		gender->modified = false;
	}
}

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Com_Printf ("User info settings:\n", LOG_CLIENT);
	Info_Print (Cvar_Userinfo());
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
void CL_Snd_Restart_f (void)
{
	S_Shutdown ();

	if (Cmd_Argc() == 1)
		S_Init (-1);
	else
		S_Init (atoi(Cmd_Argv(1)));

	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	CL_RegisterSounds ();
}

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;
int precache_model_skin;
uint32 precache_start_time;

static byte *precache_model; // used for skin checking in alias models

//#define	PLAYER_MULT	5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT+13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

typedef struct cl_location_s
{
	struct cl_location_s	*next;
	char					*name;
	vec3_t					location;
} cl_location_t;

static cl_location_t	cl_locations;

void CL_Loc_Init (void)
{
	memset (&cl_locations, 0, sizeof(cl_locations));
}

void CL_FreeLocs (void)
{
	cl_location_t	*loc = &cl_locations,
					*old = NULL;

	while (loc->next)
	{
		loc = loc->next;
		if (old)
		{
			Z_Free (old->name);
			Z_Free (old);
		}
		old = loc;
	}

	if (old)
	{
		Z_Free (old->name);
		Z_Free (old);
	}

	cl_locations.next = NULL;
}

//you wanted locs, you got em... dear god this is terrible code :)
qboolean CL_LoadLoc (const char *filename)
{
	char	*locBuffer;
	cl_location_t	*loc = &cl_locations;
	char *x, *y, *z, *name, *line;
	int	linenum;
	int len;
	FILE	*handle;
	qboolean	closeFile;

	CL_FreeLocs ();

	len = FS_LoadFile ((char *)filename, NULL);

	if (len == -1)
	{
		Com_DPrintf ("CL_LoadLoc: %s not found\n", filename);
		return false;
	}

	FS_FOpenFile (filename, &handle, HANDLE_OPEN, &closeFile);
	if (!handle)
	{
		Com_Printf ("CL_LoadLoc: couldn't open %s\n", LOG_CLIENT|LOG_WARNING, filename);
		return false;
	}

	locBuffer = Z_TagMalloc (len+2, TAGMALLOC_CLIENT_LOC);
	//locBuffer = alloca (len+2);
	FS_Read (locBuffer, len, handle);

	if (closeFile)
		FS_FCloseFile (handle);

	//terminate if no EOL
	locBuffer[len-1] = '\n';
	locBuffer[len] = 0;

	linenum = 0;

	line = locBuffer;

	for (;;)
	{
		//eof
		if (line - locBuffer >= len)
			break;

		//skip whitespace
		while (*line && (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n'))
		{
			line++;
			if (line - locBuffer >= len)
				break;
		}

		//eof now?
		if (line - locBuffer >= len)
			break;

		linenum++;
		x = line;

		name = y = z = NULL;

		while (*line)
		{
			if (*line == ' ' || *line == '\t') {
				*line++ = '\0';
				y = line;
				break;
			}
			line++;
			if (line - locBuffer >= len)
				break;
		}

		while (*line)
		{
			if (*line == ' ' || *line == '\t') {
				*line++ = '\0';
				z = line;
				break;
			}
			line++;
			if (line - locBuffer >= len)
				break;
		}

		if (line - locBuffer >= len)
			break;

		while (*line)
		{
			if (*line == ' ' || *line == '\t')
			{
				*line++ = '\0';
				name = line;
				break;
			}
			line++;
			if (line - locBuffer >= len)
				break;
		}

		if (line - locBuffer >= len)
			break;

		while (*line)
		{
			if (*line == '\n' || *line == '\r') {
				*line = '\0';
				break;
			}
			line++;
			if (line - locBuffer >= len)
				break;
		}

		if (!*x || !y || !*y || !z || !*z || !name || !*name)
		{
			Com_Printf ("CL_LoadLoc: Parse error on line %d of '%s'.\n", LOG_CLIENT, linenum, filename);
			break;
		}

		loc->next = Z_TagMalloc (sizeof(cl_location_t), TAGMALLOC_CLIENT_LOC);
		loc = loc->next;

		loc->next = NULL;
		loc->name = CopyString (name, TAGMALLOC_CLIENT_LOC);
		VectorSet (loc->location, (float)atoi(x)/8.0f, (float)atoi(y)/8.0f, (float)atoi(z)/8.0f);

		//Com_DPrintf ("CL_AddLoc: adding location '%s'\n", name);

		//advance past \0
		line++;
	}

	Com_Printf ("Read %d map locations from '%s'\n", LOG_CLIENT, linenum, filename);

	Z_Free (locBuffer);
	return true;
}

const char *CL_Loc_Get (vec3_t org)
{
	vec3_t			distance;
	uint32			length, bestlength = 0xFFFFFFFF;
	cl_location_t	*loc = &cl_locations, *best = &cl_locations;

	Q_assert (cl_locations.next);

	while (loc->next)
	{
		loc = loc->next;

		VectorSubtract (loc->location, org, distance);
		length = (int)VectorLength (distance);

		if (length < bestlength)
		{
			best = loc;
			bestlength = length;
		}
	}

	return best->name;
}

void CL_AddLoc_f (void)
{
	cl_location_t	*loc, *last, *newentry;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Add a new location.\n"
					"Syntax : addloc <name>\n"
					"Example: addloc red base\n", LOG_CLIENT);
		return;
	}

	if (cls.state < ca_active)
	{
		Com_Printf ("Not connected.\n", LOG_GENERAL);
		return;
	}

	loc = &cl_locations;

	last = loc->next;

	newentry = Z_TagMalloc (sizeof(*loc), TAGMALLOC_CLIENT_LOC);
	newentry->name = CopyString (Cmd_Args(), TAGMALLOC_CLIENT_LOC);
	FastVectorCopy (cl.refdef.vieworg, newentry->location);
	newentry->next = last;
	loc->next = newentry;

	Com_Printf ("Location '%s' added at (%d, %d, %d).\n", LOG_CLIENT, newentry->name, (int)cl.refdef.vieworg[0]*8, (int)cl.refdef.vieworg[1]*8, (int)cl.refdef.vieworg[2]*8);
}

void CL_SaveLoc_f (void)
{
	cl_location_t	*loc = &cl_locations;
	FILE			*out;

	char			locbuff[MAX_QPATH+4];
	char			locfile[MAX_OSPATH];

	if (!cl_locations.next)
	{
		Com_Printf ("No locations defined.\n", LOG_GENERAL);
		return;
	}

	COM_StripExtension (cl.configstrings[CS_MODELS+1], locbuff);
	strcat (locbuff, ".loc");

	Com_sprintf (locfile, sizeof(locfile), "%s/%s", FS_Gamedir(), locbuff);

	out = fopen (locfile, "wb");
	if (!out)
	{
		Com_Printf ("CL_SaveLoc_f: Unable to open '%s' for writing.\n", LOG_CLIENT, locfile);
		return;
	}

	while (loc->next)
	{
		loc = loc->next;
		fprintf (out, "%i %i %i %s\n", (int)loc->location[0]*8, (int)loc->location[1]*8, (int)loc->location[2]*8, loc->name);
	}

	fclose (out);
	Com_Printf ("Locations saved to '%s'.\n", LOG_CLIENT, locbuff);
}

void vectoangles2 (vec3_t value1, vec3_t angles);

const char *CL_Get_Loc_There (void)
{
	trace_t		tr;
	vec3_t		end;

	if (!cl_locations.next)
		return cl_default_location->string;

	end[0] = cl.refdef.vieworg[0] + cl.v_forward[0] * 65556 + cl.v_right[0];
	end[1] = cl.refdef.vieworg[1] + cl.v_forward[1] * 65556 + cl.v_right[1];
	end[2] = cl.refdef.vieworg[2] + cl.v_forward[2] * 65556 + cl.v_right[2];

	tr = CM_BoxTrace (cl.refdef.vieworg, end, vec3_origin, vec3_origin, 0, MASK_SOLID);

	if (tr.fraction != 1.0f)
		return CL_Loc_Get (tr.endpos);

	return CL_Loc_Get (end);
}

const char *CL_Get_Loc_Here (void)
{
	if (!cl_locations.next)
		return cl_default_location->string;

	return CL_Loc_Get (cl.refdef.vieworg);
}

/*void CL_Say_Preprocessor (void)
{
	char *location_name, *p;
	char *say_text;
	
	say_text = p = Cmd_Args();

	if (cl_locations.next)
	{
		while (say_text[0] && say_text[1])
		{
			if ((say_text[0] == '%' && say_text[1] == 'l') || (say_text[0] == '@' && say_text[1] == 't'))
			{
				int location_len, cmd_len;

				if (say_text[1] == 'l')
				{
					location_name = CL_Loc_Get (cl.refdef.vieworg);
				}
				else
				{
					trace_t		tr;
					vec3_t		end;

					end[0] = cl.refdef.vieworg[0] + cl.v_forward[0] * 65556 + cl.v_right[0];
					end[1] = cl.refdef.vieworg[1] + cl.v_forward[1] * 65556 + cl.v_right[1];
					end[2] = cl.refdef.vieworg[2] + cl.v_forward[2] * 65556 + cl.v_right[2];

					tr = CM_BoxTrace (cl.refdef.vieworg, end, vec3_origin, vec3_origin, 0, MASK_SOLID);

					if (tr.fraction != 1.0f)
						location_name = CL_Loc_Get (tr.endpos);
					else
						location_name = CL_Loc_Get (end);
				}

				cmd_len = (int)strlen(Cmd_Args());
				location_len = (int)strlen(location_name);

				if (cmd_len + location_len >= MAX_STRING_CHARS-1)
				{
					Com_DPrintf ("CL_Say_Preprocessor: location expansion aborted, not enough space\n");
					break;
				}

				memmove (say_text + location_len, say_text + 2, cmd_len - (say_text - p) - 1);
				memcpy (say_text, location_name, location_len);
				say_text += location_len;
			}
			say_text++;
		}
	}

	Cmd_ForwardToServer ();
}*/

const char *colortext(const char *text)
{
	static char ctext[2][80];
	static int c=0;
	const char *p;
	char *cp;
	c^=1;
	if (!text[0])
		return text;

	for (p=text, cp=ctext[c];p[0]!=0;p++, cp++){
		cp[0]=p[0]|128;
	}
	cp[0]=0;
	return ctext[c];
}

void CL_ResetPrecacheCheck (void)
{
	precache_start_time = Sys_Milliseconds();

	precache_check = CS_MODELS;
	precache_model = 0;
	precache_model_skin = -1;
}

void CL_RequestNextDownload (void)
{
	int			PLAYER_MULT;
	const char	*sexedSounds[MAX_SOUNDS];
	uint32		map_checksum;		// for detecting cheater maps
	char		fn[MAX_OSPATH];

	char		*cmd;
	dmdl_t		*pheader;
	dsprite_t	*spriteheader;

	int			i;

	if (cls.state != ca_connected)
		return;

	PLAYER_MULT = 0;

	for (i = CS_SOUNDS; i < CS_SOUNDS + MAX_SOUNDS; i++)
	{
		if (cl.configstrings[i][0] == '*')// && !strstr (cl.configstrings[i], ".."))
			sexedSounds[PLAYER_MULT++] = cl.configstrings[i] + 1;
	}

	PLAYER_MULT += 5;

	if (!allow_download->intvalue && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

//ZOID
	if (precache_check == CS_MODELS)
	{ // confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->intvalue)
		{
			if (strlen(cl.configstrings[CS_MODELS+1]) >= MAX_QPATH-1)
				Com_Error (ERR_DROP, "Bad map configstring '%s'", cl.configstrings[CS_MODELS+1]);

			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
				return; // started a download
		}
	}


redoSkins:;

	if (precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS)
	{
		if (allow_download_models->intvalue)
		{
			char *skinname;

			while (precache_check < CS_MODELS+MAX_MODELS &&
				cl.configstrings[precache_check][0])
			{
				//its a brush/alias model, we don't do those
				if (cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#')
				{
					precache_check++;
					continue;
				}

				//new model, try downloading it
				if (precache_model_skin == -1)
				{
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check]))
					{
						precache_check++;
						return; // started a download
					}
					precache_check++;
				}
				else
				{
					//model is ok, now we are checking for skins in the model
					if (!precache_model)
					{
						//load model into buffer
						precache_model_skin = 1;
						FS_LoadFile (cl.configstrings[precache_check], (void **)&precache_model);
						if (!precache_model)
						{
							//shouldn't happen?
							precache_model_skin = 0;
							precache_check++;
							continue; // couldn't load it
						}
						
						//is it an alias model
						if (LittleLong(*(uint32 *)precache_model) != IDALIASHEADER)
						{
							//is it a sprite
							if (LittleLong(*(uint32 *)precache_model) != IDSPRITEHEADER)
							{
								//no, free and move onto next model
								FS_FreeFile(precache_model);
								precache_model = NULL;
								precache_model_skin = 0;
								precache_check++;
								continue;
							}
							else
							{
								//get sprite header
								spriteheader = (dsprite_t *)precache_model;
								if (LittleLong (spriteheader->version != SPRITE_VERSION))
								{
									//this is unknown version! free and move onto next.
									FS_FreeFile(precache_model);
									precache_model = NULL;
									precache_check++;
									precache_model_skin = 0;
									continue; // couldn't load it
								}
							}
						}
						else
						{
							//get model header
							pheader = (dmdl_t *)precache_model;
							if (LittleLong (pheader->version) != ALIAS_VERSION)
							{
								//unknown version! free and move onto next
								FS_FreeFile(precache_model);
								precache_model = NULL;
								precache_check++;
								precache_model_skin = 0;
								continue; // couldn't load it
							}
						}
					}

					//if its an alias model
					if (LittleLong(*(uint32 *)precache_model) == IDALIASHEADER)
					{
						pheader = (dmdl_t *)precache_model;

						//iterate through number of skins
						while (precache_model_skin - 1 < LittleLong(pheader->num_skins))
						{
							skinname = (char *)precache_model +
								LittleLong(pheader->ofs_skins) + 
								(precache_model_skin - 1)*MAX_SKINNAME;

							//r1: spam warning for models that are broken
							if (strchr (skinname, '\\'))
							{
								Com_Printf ("Warning, model %s with incorrectly linked skin: %s\n", LOG_CLIENT|LOG_WARNING, cl.configstrings[precache_check], skinname);
							}
							else if (strlen(skinname) > MAX_SKINNAME-1)
							{
								Com_Error (ERR_DROP, "Model %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);
							}

							//check if this skin exists
							if (!CL_CheckOrDownloadFile(skinname))
							{
								precache_model_skin++;
								return; // started a download
							}
							precache_model_skin++;
						}
					}
					else
					{
						//its a sprite
						spriteheader = (dsprite_t *)precache_model;

						//iterate through skins
						while (precache_model_skin - 1 < LittleLong(spriteheader->numframes))
						{
							skinname = spriteheader->frames[(precache_model_skin - 1)].name;

							//r1: spam warning for models that are broken
							if (strchr (skinname, '\\'))
							{
								Com_Printf ("Warning, sprite %s with incorrectly linked skin: %s\n", LOG_CLIENT|LOG_WARNING, cl.configstrings[precache_check], skinname);
							}
							else if (strlen(skinname) > MAX_SKINNAME-1)
							{
								Com_Error (ERR_DROP, "Sprite %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);
							}

							//check if this skin exists
							if (!CL_CheckOrDownloadFile(skinname))
							{
								precache_model_skin++;
								return; // started a download
							}
							precache_model_skin++;
						}
					}

					//we're done checking the model and all skins, free
					if (precache_model)
					{ 
						FS_FreeFile(precache_model);
						precache_model = NULL;
					}

					precache_model_skin = 0;
					precache_check++;
				}
			}
		}
		if (precache_model_skin == -1)
		{
			precache_check = CS_MODELS + 2;
			precache_model_skin = 0;

	//pending downloads (models), let's wait here before we can check skins.
#ifdef USE_CURL
		if (CL_PendingHTTPDownloads ())
			return;
#endif

			goto redoSkins;
		}
		precache_check = CS_SOUNDS;
	}

	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS)
	{ 
		if (allow_download_sounds->intvalue)
		{
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank

			while (precache_check < CS_SOUNDS+MAX_SOUNDS &&
				cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*')
				{
					precache_check++;
					continue;
				}

				if (cl.configstrings[precache_check][0] == '#')
					Com_sprintf(fn, sizeof(fn), "%s", cl.configstrings[precache_check++] + 1);
				else
					Com_sprintf(fn, sizeof(fn), "sound/%s", cl.configstrings[precache_check++]);

				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = CS_IMAGES;
	}

	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES+MAX_IMAGES)
	{
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank

		while (precache_check < CS_IMAGES+MAX_IMAGES && cl.configstrings[precache_check][0])
		{
			Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check]);
			if (FS_LoadFile (fn, NULL) == -1)
			{
				//Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check]);
				if (!CL_CheckOrDownloadFile(fn))
				{
					precache_check++;
					return; // started a download
				}
			}
			precache_check++;
		}
		precache_check = CS_PLAYERSKINS;
	}

	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
	{
		if (allow_download_players->intvalue)
		{
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
			{
				int i, n, j, length;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS)%PLAYER_MULT;

				if (i >= cl.maxclients)
				{
					precache_check = ENV_CNT;
					continue;
				}

				if (!cl.configstrings[CS_PLAYERSKINS+i][0])
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if (n && cls.failed_download)
				{
					cls.failed_download = false;
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS+i], '\\')) != NULL)
				{
					p++;
				}
				else
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				Q_strncpy(model, p, sizeof(model)-1);

				p = strchr(model, '/');

				if (!p)
					p = strchr(model, '\\');

				if (p)
				{
					*p++ = 0;
					if (!p[0] || !model[0])
					{
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
						continue;
					}
					else
					{
						Q_strncpy (skin, p, sizeof(skin)-1);
					}
				}
				else
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}
					//*skin = 0;

				length = (int)strlen (model);
				for (j = 0; j < length; j++)
				{
					if (!isvalidchar(model[j]))
					{
						Com_Printf ("Bad character '%c' in playermodel '%s'\n", LOG_CLIENT|LOG_WARNING, model[j], model);
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
						goto skipplayer;
					}
				}

				length = (int)strlen (skin);
				for (j = 0; j < length; j++)
				{
					if (!isvalidchar(skin[j]))
					{
						Com_Printf ("Bad character '%c' in playerskin '%s'\n", LOG_CLIENT|LOG_WARNING, skin[j], skin);
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
						goto skipplayer;
					}
				}

				switch (n) 
				{
					case -1:
						continue;
					case 0: // model
						cls.failed_download = false;
						Com_sprintf(fn, sizeof(fn), "players/%s/tris.md2", model);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 1: // weapon model
						Com_sprintf(fn, sizeof(fn), "players/%s/weapon.md2", model);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 2: // weapon skin
						Com_sprintf(fn, sizeof(fn), "players/%s/weapon.pcx", model);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 3: // skin
						Com_sprintf(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 4: // skin_i
						Com_sprintf(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
							return; // started a download
						}
						n = 5;

					default:
						while (n < PLAYER_MULT)
						{
							Com_sprintf(fn, sizeof(fn), "players/%s/%s", model, sexedSounds[n-5]);
							n++;
							if (!CL_CheckOrDownloadFile(fn)) {
								precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + n;
								return; // started a download
							}
						}
				}
				
				// move on to next model
				precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;

skipplayer:;
			}
		}
		// precache phase completed
		precache_check = ENV_CNT;
	}

#ifdef USE_CURL
		//map might still be downloading?
		if (CL_PendingHTTPDownloads ())
			return;
#endif

 	if (precache_check == ENV_CNT)
	{
		char locbuff[MAX_QPATH+4];
		precache_check = ENV_CNT + 1;

		COM_StripExtension (cl.configstrings[CS_MODELS+1], locbuff);
		strcat (locbuff, ".loc");

		//try basedir/moddir/maps/mapname.loc and basedir/locs/mapname.loc
		if (!CL_LoadLoc (locbuff))
		{
			locbuff[0] = 'l';
			locbuff[1] = 'o';
			locbuff[2] = 'c';
			locbuff[3] = 's';
			CL_LoadLoc (locbuff);
		}

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);

		if (map_checksum && map_checksum != strtoul(cl.configstrings[CS_MAPCHECKSUM], NULL, 10)) {
			Com_Error (ERR_DROP, "Local map version differs from server: 0x%.8x != 0x%.8x",
				map_checksum, atoi(cl.configstrings[CS_MAPCHECKSUM]));
			return;
		}
	}

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT) {
		if (allow_download->intvalue && allow_download_maps->intvalue) {
			while (precache_check < TEXTURE_CNT) {
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1)
					Com_sprintf(fn, sizeof(fn), "env/%s%s.pcx", 
						cl.configstrings[CS_SKY], env_suf[n/2]);
				else
					Com_sprintf(fn, sizeof(fn), "env/%s%s.tga", 
						cl.configstrings[CS_SKY], env_suf[n/2]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT) {
		precache_check = TEXTURE_CNT+1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT+1) {
		// from qcommon/cmodel.c
		extern int			numtexinfo;
		extern mapsurface_t	map_surfaces[];

		if (allow_download->intvalue && allow_download_maps->intvalue) {
			while (precache_tex < numtexinfo) {
				//char fn[MAX_OSPATH];

				//r1: spam warning about texture
				if (strchr (map_surfaces[precache_tex].rname, '\\'))
					Com_Printf ("Warning, incorrectly referenced texture '%s'\n", LOG_CLIENT|LOG_WARNING, map_surfaces[precache_tex].rname);
				else if (!strchr (map_surfaces[precache_tex].rname, '/'))
					Com_Printf ("Warning, texture '%s' not in a sub-directory\n", LOG_CLIENT|LOG_WARNING, map_surfaces[precache_tex].rname);

				Com_sprintf(fn, sizeof(fn), "textures/%s.wal", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT+999;
	}

	//pending downloads (possibly textures), let's wait here.
#ifdef USE_CURL
	if (CL_PendingHTTPDownloads ())
		return;
#endif

//ZOID
	CL_RegisterSounds ();
	CL_PrepRefresh ();

	Com_DPrintf ("Precache completed in %u msec.\n", Sys_Milliseconds() - precache_start_time);

	if (cl_async->intvalue && cl_maxfps->intvalue > 100)
		Com_Printf ("\n%s\nR1Q2 separates network and rendering so a cl_maxfps value > 30\nis rarely needed. Use r_maxfps to control maximum rendered FPS\n\n", LOG_CLIENT, colortext ("WARNING: A cl_maxfps value of over 100 is strongly discouraged"));

	if (cl_autorecord->intvalue)
	{
		char	autorecord_name[MAX_QPATH];
		char	mapname[MAX_QPATH];
		char	time_buff[32];

		time_t	tm;

		time(&tm);
		strftime(time_buff, sizeof(time_buff)-1, "%Y-%m-%d-%H%M", localtime(&tm));

		COM_StripExtension (cl.configstrings[CS_MODELS+1], mapname);

		Com_sprintf (autorecord_name, sizeof(autorecord_name), "%s/demos/%s-%s.dm2", FS_Gamedir(), time_buff, mapname + 5);
		
		if (CL_BeginRecording (autorecord_name))
			Com_Printf ("Auto-recording to %s.\n", LOG_CLIENT, autorecord_name);
		else
			Com_Printf ("Couldn't auto-record to %s.\n", LOG_CLIENT, autorecord_name);
	}

	CL_FixCvarCheats();

	if (cls.serverProtocol == PROTOCOL_R1Q2)
	{
		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_NOGUN);
		MSG_WriteShort (cl_gun->intvalue ? 0 : 1);
		MSG_EndWriting (&cls.netchan.message);

		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_PLAYERUPDATE_REQUESTS);
		MSG_WriteShort (cl_player_updates->intvalue);
		MSG_EndWriting (&cls.netchan.message);

		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_FPS);
		MSG_WriteShort (cl_updaterate->intvalue);
		MSG_EndWriting (&cls.netchan.message);
	}

	MSG_WriteByte (clc_stringcmd);
	MSG_WriteString (va("begin %i\n", precache_spawncount) );
	MSG_EndWriting (&cls.netchan.message);

	cmd = Cmd_MacroExpandString("$cl_beginmapcmd");
	if (cmd)
	{
		Cbuf_AddText (cmd);
		Cbuf_AddText ("\n");
		Cbuf_Execute ();
	}
	else
		Com_Printf ("WARNING: Error expanding $cl_beginmapcmd, ignored.\n", LOG_CLIENT|LOG_WARNING);

	send_packet_now = true;
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	//Yet another hack to let old demos work
	//the old precache sequence
	if (Cmd_Argc() < 2)
	{
		uint32		map_checksum;		// for detecting cheater maps
		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);
		CL_RegisterSounds ();
		CL_PrepRefresh ();
		return;
	}

	precache_spawncount = atoi(Cmd_Argv(1));
	CL_ResetPrecacheCheck ();
	CL_RequestNextDownload();
}

void CL_Toggle_f (void)
{
	cvar_t *tvar;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Usage: toggle cvar [option1 option2 option3 ...]\n", LOG_CLIENT);
		return;
	}

	tvar = Cvar_FindVar (Cmd_Argv(1));

	if (!tvar)
	{
		Com_Printf ("%s: no such variable\n", LOG_CLIENT, Cmd_Argv(1));
		return;
	}

	if (Cmd_Argc() == 2)
	{
		if (tvar->value != 0 && tvar->value != 1)
		{
			Com_Printf ("%s: not a binary variable\n", LOG_CLIENT, Cmd_Argv(1));
			return;
		}
		Cvar_SetValue (Cmd_Argv(1), (float)((int)tvar->value ^ 1));
	}
	else
	{
		int i;

		for (i = 2; i < Cmd_Argc(); i++)
		{
			if (!strcmp (tvar->string, Cmd_Argv(i)))
			{
				if (i == Cmd_Argc() -1)
				{
					Cvar_Set (Cmd_Argv(1), Cmd_Argv(2));
					break;
				}
				else
				{
					Cvar_Set (Cmd_Argv(1), Cmd_Argv(i+1));
					break;
				}
			}
		}
	}
}

void version_update (cvar_t *self, char *old, char *newValue)
{
	Cvar_Set ("cl_version", va(PRODUCTNAME " " VERSION "; %s", newValue[0] ? newValue : "unknown renderer"));
}

void _name_changed (cvar_t *var, char *oldValue, char *newValue)
{
	if (strlen(newValue) >= 16)
	{
		newValue[15] = 0;
	}
	else if (!*newValue)
	{
		Cvar_Set ("name", "unnamed");
	}
}

void _maxfps_changed (cvar_t *var, char *oldValue, char *newValue)
{
	if (var->intvalue < 5)
	{
		Cvar_Set (var->name, "5");
	}

	if (cl_async->intvalue == 0)
	{
		if (var == cl_maxfps)
			Cvar_SetValue ("r_maxfps", var->value);
		else if (var == r_maxfps)
			Cvar_SetValue ("cl_maxfps", var->value);
	}
}

void _async_changed (cvar_t *var, char *oldValue, char *newValue)
{
	if (var->intvalue == 0)
	{
		Cvar_SetValue ("r_maxfps", cl_maxfps->value);
	}
}

void _railtrail_changed (cvar_t *var, char *oldValue, char *newValue)
{
	if (var->intvalue > 254)
		Cvar_Set (var->name, "254");
	else if (var->intvalue < 0)
		Cvar_Set (var->name, "0");
}

void _protocol_changed (cvar_t *var, char *oldValue, char *newValue)
{
	//force reparsing of cl_protocol
	if (cls.state == ca_disconnected)
		cls.serverProtocol = 0;
}

void CL_FollowIP_f (void)
{
	if (!cls.followHost[0])
	{
		Com_Printf ("No IP to follow.\n", LOG_GENERAL);
		return;
	}

	Cbuf_AddText (va("connect \"%s\"\n", cls.followHost));
}

void CL_IndexStats_f (void)
{
	int	i;
	int	count;
/*
#define	CS_SOUNDS			(CS_MODELS+MAX_MODELS)			//288
#define	CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)			//544
#define	CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)			//800
#define	CS_ITEMS			(CS_LIGHTS+MAX_LIGHTSTYLES)		//1056
#define	CS_PLAYERSKINS		(CS_ITEMS+MAX_ITEMS)			//1312
#define CS_GENERAL			(CS_PLAYERSKINS+MAX_CLIENTS)	//1568
#define	MAX_CONFIGSTRINGS	(CS_GENERAL+MAX_GENERAL)		//2080
*/
	if (Cmd_Argc() != 1)
	{
		unsigned	offset;
		unsigned	count = atoi(Cmd_Argv(2));

		if (!Q_stricmp (Cmd_Argv(1), "cs_models"))
			offset = CS_MODELS;
		else if (!Q_stricmp (Cmd_Argv(1), "cs_sounds"))
			offset = CS_SOUNDS;
		else if (!Q_stricmp (Cmd_Argv(1), "cs_images"))
			offset = CS_IMAGES;
		else if (!Q_stricmp (Cmd_Argv(1), "cs_items"))
			offset = CS_ITEMS;
		else if (!Q_stricmp (Cmd_Argv(1), "cs_playerskins"))
			offset = CS_PLAYERSKINS;
		else
			offset = atoi(Cmd_Argv(1));

		if (offset >= CS_GENERAL + MAX_GENERAL)
		{
			Com_Printf ("Bad offset.\n", LOG_GENERAL);
			return;
		}

		if (count == 0)
			count = 256;

		if (offset + count > CS_GENERAL + MAX_GENERAL)
		{
			count = CS_GENERAL + MAX_GENERAL - offset;
		}

		for (i = offset; i < offset + count; i++)
		{
			Com_Printf ("%3d/%3d: %s\n", LOG_GENERAL, i, i - offset, cl.configstrings[i]);
		}
		
		return;
	}
	for (count = 0, i = CS_MODELS; i < CS_MODELS + MAX_MODELS; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_MODELS     : %d\n", LOG_GENERAL, count);

	for (count = 0, i = CS_SOUNDS; i < CS_SOUNDS + MAX_SOUNDS; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_SOUNDS     : %d\n", LOG_GENERAL, count);

	for (count = 0, i = CS_LIGHTS; i < CS_LIGHTS + MAX_LIGHTSTYLES; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_LIGHTS     : %d\n", LOG_GENERAL, count);

	for (count = 0, i = CS_IMAGES; i < CS_IMAGES + MAX_IMAGES; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_IMAGES     : %d\n", LOG_GENERAL, count);

	for (count = 0, i = CS_ITEMS; i < CS_ITEMS + MAX_ITEMS; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_ITEMS      : %d\n", LOG_GENERAL, count);

	for (count = 0, i = CS_PLAYERSKINS; i < CS_PLAYERSKINS + MAX_CLIENTS; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_PLAYERSKINS: %d\n", LOG_GENERAL, count);

	for (count = 0, i = CS_GENERAL; i < CS_GENERAL + MAX_GENERAL; i++)
	{
		if (cl.configstrings[i][0])
			count++;
	}
	Com_Printf ("CS_GENERAL    : %d\n", LOG_GENERAL, count);
}

void _cl_http_max_connections_changed (cvar_t *c, char *old, char *new)
{
	if (c->intvalue > 8)
		Cvar_Set (c->name, "8");
	else if (c->intvalue < 1)
		Cvar_Set (c->name, "1");

	//not really needed any more, hopefully no one still uses apache...
	//if (c->intvalue > 2)
	//	Com_Printf ("WARNING: Changing the maximum connections higher than 2 violates the HTTP specification recommendations. Doing so may result in you being blocked from the remote system and offers no performance benefits unless you are on a very high latency link (ie, satellite)\n", LOG_GENERAL);
}

void _gun_changed (cvar_t *c, char *old, char *new)
{
	if (cls.state >= ca_connected && cls.serverProtocol == PROTOCOL_R1Q2)
	{
		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_NOGUN);
		MSG_WriteShort (c->intvalue ? 0 : 1);
		MSG_EndWriting (&cls.netchan.message);
	}
}

void _player_updates_changed (cvar_t *c, char *old, char *new)
{
	if (c->intvalue > 10)
		Cvar_Set (c->name, "10");
	else if (c->intvalue < 0)
		Cvar_Set (c->name, "0");

	if (cls.state >= ca_connected && cls.serverProtocol == PROTOCOL_R1Q2)
	{	
		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_PLAYERUPDATE_REQUESTS);
		MSG_WriteShort (cl_player_updates->intvalue);
		MSG_EndWriting (&cls.netchan.message);
	}

	if (c->intvalue == 0)
		cl.player_update_time = 0;
}

void _updaterate_changed (cvar_t *c, char *old, char *new)
{
	if (c->intvalue < 0)
		Cvar_Set (c->name, "0");

	if (cls.state >= ca_connected && cls.serverProtocol == PROTOCOL_R1Q2)
	{	
		MSG_BeginWriting (clc_setting);
		MSG_WriteShort (CLSET_FPS);
		MSG_WriteShort (cl_updaterate->intvalue);
		MSG_EndWriting (&cls.netchan.message);
	}
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	const char	*glVersion;

	cls.proxyState = ps_none;
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds ();

	CL_InitInput ();

	adr0 = Cvar_Get( "adr0", "", CVAR_ARCHIVE );
	adr1 = Cvar_Get( "adr1", "", CVAR_ARCHIVE );
	adr2 = Cvar_Get( "adr2", "", CVAR_ARCHIVE );
	adr3 = Cvar_Get( "adr3", "", CVAR_ARCHIVE );
	adr4 = Cvar_Get( "adr4", "", CVAR_ARCHIVE );
	adr5 = Cvar_Get( "adr5", "", CVAR_ARCHIVE );
	adr6 = Cvar_Get( "adr6", "", CVAR_ARCHIVE );
	adr7 = Cvar_Get( "adr7", "", CVAR_ARCHIVE );
	adr8 = Cvar_Get( "adr8", "", CVAR_ARCHIVE );

//
// register our variables
//
#ifdef CL_STEREO_SUPPORT
	cl_stereo_separation = Cvar_Get( "cl_stereo_separation", "0.4", CVAR_ARCHIVE );
	cl_stereo = Cvar_Get( "cl_stereo", "0", 0 );
#endif

	cl_add_blend = Cvar_Get ("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get ("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get ("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get ("cl_entities", "1", 0);
	cl_gun = Cvar_Get ("cl_gun", "1", CVAR_ARCHIVE);
	cl_gun->changed = _gun_changed;

	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	cl_noskins = Cvar_Get ("cl_noskins", "0", 0);
//	cl_autoskins = Cvar_Get ("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get ("cl_predict", "1", 0);
	cl_backlerp = Cvar_Get ("cl_backlerp", "1", 0);
//	cl_minfps = Cvar_Get ("cl_minfps", "5", 0);

	r_maxfps = Cvar_Get ("r_maxfps", "1000", 0);
	r_maxfps->changed = _maxfps_changed;

	cl_maxfps = Cvar_Get ("cl_maxfps", "60", CVAR_ARCHIVE);
	cl_maxfps->changed = _maxfps_changed;

	cl_async = Cvar_Get ("cl_async", "1", 0);
	cl_async->changed = _async_changed;

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0);
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_run = Cvar_Get ("cl_run", "1", CVAR_ARCHIVE);

	freelook = Cvar_Get( "freelook", "1", CVAR_ARCHIVE );
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0);
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get ("showclamp", "0", 0);
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);
	cl_paused = Cvar_Get ("paused", "0", 0);
	cl_timedemo = Cvar_Get ("timedemo", "0", 0);

	cl_filterchat = Cvar_Get ("cl_filterchat", "0", 0);

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", CVAR_NOSET);

	//
	// userinfo
	//
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	name = Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	skin = Cvar_Get ("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	rate = Cvar_Get ("rate", "15000", CVAR_USERINFO | CVAR_ARCHIVE);	// FIXME
	msg = Cvar_Get ("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	fov = Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	gender = Cvar_Get ("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	gender_auto = Cvar_Get ("gender_auto", "1", CVAR_ARCHIVE);
	gender->modified = false; // clear this so we know when user sets it manually

	cl_vwep = Cvar_Get ("cl_vwep", "1", CVAR_ARCHIVE);

	//r1: only use experimental protocol in debug (not)
#ifdef _DEBUG
	cl_protocol = Cvar_Get ("cl_protocol", "0", 0);
#else
	cl_protocol = Cvar_Get ("cl_protocol", "0", 0);
#endif

	cl_protocol->changed = _protocol_changed;

	//cl_defertimer = Cvar_Get ("cl_defertimer", "1", 0);
	cl_smoothsteps = Cvar_Get ("cl_smoothsteps", "3", 0);
	cl_instantpacket = Cvar_Get ("cl_instantpacket", "1", 0);
	cl_nolerp = Cvar_Get ("cl_nolerp", "0", 0);
	cl_instantack = Cvar_Get ("cl_instantack", "0", 0);
	cl_autorecord = Cvar_Get ("cl_autorecord", "0", 0);

	cl_railtrail = Cvar_Get ("cl_railtrail", "0", 0);
	cl_railtrail->changed = _railtrail_changed;

	//misc for testing
	cl_test = Cvar_Get ("cl_test", "0", 0);
	cl_test2 = Cvar_Get ("cl_test2", "0", 0);
	cl_test3 = Cvar_Get ("cl_test3", "0", 0);

	cl_original_dlights = Cvar_Get ("cl_original_dlights", "1", 0);

	cl_default_location = Cvar_Get ("cl_default_location", "", 0);

#ifdef _DEBUG
	cl_player_updates = Cvar_Get ("cl_player_updates", "0", 0);
#else
	cl_player_updates = Cvar_Get ("cl_player_updates", "0", CVAR_NOSET);
#endif

	cl_player_updates->changed = _player_updates_changed;

	cl_updaterate = Cvar_Get ("cl_updaterate", "0", 0);
	cl_updaterate->changed = _updaterate_changed;

#ifdef NO_SERVER
	allow_download = Cvar_Get ("allow_download", "1", CVAR_ARCHIVE);
	allow_download_players  = Cvar_Get ("allow_download_players", "1", CVAR_ARCHIVE);
	allow_download_models = Cvar_Get ("allow_download_models", "1", CVAR_ARCHIVE);
	allow_download_sounds = Cvar_Get ("allow_download_sounds", "1", CVAR_ARCHIVE);
	allow_download_maps	  = Cvar_Get ("allow_download_maps", "1", CVAR_ARCHIVE);
#endif

#ifdef USE_CURL
	cl_http_proxy = Cvar_Get ("cl_http_proxy", "", 0);
	cl_http_filelists = Cvar_Get ("cl_http_filelists", "1", 0);
	cl_http_downloads = Cvar_Get ("cl_http_downloads", "1", 0);
	cl_http_max_connections = Cvar_Get ("cl_http_max_connections", "4", 0);
	cl_http_max_connections->changed = _cl_http_max_connections_changed;
#endif

	cl_proxy = Cvar_Get ("cl_proxy", "", 0);

	//haxx
	glVersion = Cvar_VariableString ("cl_version");
	(Cvar_ForceSet ("cl_version", va(PRODUCTNAME " " VERSION "; %s", glVersion[0] ? glVersion : "unknown renderer" )))->changed = version_update;

	name->changed = _name_changed;

#ifdef _DEBUG
	dbg_framesleep = Cvar_Get ("dbg_framesleep", "0", 0);
#endif

	//cl_snaps = Cvar_Get ("cl_snaps", "1", 0);
	//cl_snaps->modified = false;
	//cl_snaps->changed = CL_SnapsMessage;

	CL_FixCvarCheats ();

	//
	// register our commands
	//

	//r1: r1q2 stuff goes here so it never overrides autocomplete order for base q2 cmds

	Cmd_AddCommand ("followip", CL_FollowIP_f);
	Cmd_AddCommand ("indexstats", CL_IndexStats_f);

	//r1: allow passive connects
	Cmd_AddCommand ("passive", CL_Passive_f);

	//r1: toggle cvar
	Cmd_AddCommand ("toggle", CL_Toggle_f);

	//r1: server status (connectionless)
	Cmd_AddCommand ("serverstatus", CL_ServerStatus_f);

	Cmd_AddCommand ("ignore", CL_Ignore_f);
	Cmd_AddCommand ("unignore", CL_Unignore_f);

	//r1: loc support
	Cmd_AddCommand ("addloc", CL_AddLoc_f);
	Cmd_AddCommand ("saveloc", CL_SaveLoc_f);

#ifdef _DEBUG
	Cmd_AddCommand ("packet", CL_Packet_f);
	Cmd_AddCommand ("spam", CL_Spam_f);
#endif

#ifdef CLIENT_DLL
	Cmd_AddCommand ("cl_restart", CL_ClDLL_Restart_f);
#endif

	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);
	Cmd_AddCommand ("skins", CL_Skins_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);

// 	Cmd_AddCommand ("packet", CL_Packet_f); // this is dangerous to leave in

	//r1: serves no purpose other than to allow buffer overflows!
//	Cmd_AddCommand ("setenv", CL_Setenv_f );

	Cmd_AddCommand ("precache", CL_Precache_f);

	Cmd_AddCommand ("download", CL_Download_f);

	//
	// forward to server commands
	//
	// the only thing this does is allow command completion
	// to work -- all unknown commands are automatically
	// forwarded to the server
	Cmd_AddCommand ("wave", NULL);
	Cmd_AddCommand ("inven", NULL);
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("use", NULL);
	Cmd_AddCommand ("drop", NULL);

	//r1: loc support
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);

	Cmd_AddCommand ("info", NULL);

	//???
	//Cmd_AddCommand ("prog", NULL);

	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("invuse", NULL);
	Cmd_AddCommand ("invprev", NULL);
	Cmd_AddCommand ("invnext", NULL);
	Cmd_AddCommand ("invdrop", NULL);
	Cmd_AddCommand ("weapnext", NULL);
	Cmd_AddCommand ("weapprev", NULL);
}



/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void CL_WriteConfiguration (void)
{
	FILE	*f;
	char	path[MAX_QPATH];

	if (cls.state == ca_uninitialized)
		return;

	Com_sprintf (path, sizeof(path),"%s/config.cfg",FS_Gamedir());
	f = fopen (path, "w");
	if (!f)
	{
		Com_Printf ("Couldn't write config.cfg.\n", LOG_CLIENT);
		return;
	}

	fprintf (f, "// generated by quake, do not modify\n");
	Key_WriteBindings (f);
	fclose (f);

	Cvar_WriteVariables (path);
}


/*
==================
CL_FixCvarCheats

==================
*/

typedef struct
{
	char	*name;
	float	value;
	float	setval;
	cvar_t	*var;
} cheatvar_t;

cheatvar_t	cheatvars[] = {
	{"timescale", 1, 1, NULL},
	{"timedemo", 0, 0, NULL},
	{"r_drawworld", 1, 1, NULL},
	{"cl_testlights", 0, 0, NULL},
	{"cl_testblend", 0, 0, NULL},
	{"r_fullbright", 0, 0, NULL},
	{"r_drawflat", 0, 0, NULL},
	{"paused", 0, 0, NULL},
	{"fixedtime", 0, 0, NULL},
	{"sw_draworder", 0, 0, NULL},
	{"gl_lightmap", 0, 0, NULL},
	{"gl_saturatelighting", 0, 0, NULL},
	{"gl_lockpvs", 0, 0, NULL},
	{"sw_lockpvs", 0, 0, NULL},
	{NULL, 0, 0, NULL}
};

int		numcheatvars;

void _cheatcvar_changed (cvar_t *cvar, char *oldValue, char *newValue)
{
	int			i;
	cheatvar_t	*var;

#ifdef _DEBUG
	return;
#endif

	if (cls.state == ca_disconnected || cl.attractloop || Com_ServerState() == ss_demo || cl.maxclients == 1)
		return;		// single player can cheat

	for (i=0, var = cheatvars ; i<numcheatvars ; i++, var++)
	{
		if (var->var == cvar && var->value != cvar->value)
		{
			Com_Printf ("%s is cheat protected.\n", LOG_GENERAL, var->name);
			Cvar_SetValue (var->name, var->setval);
			return;
		}
	}
}

void CL_FixCvarCheats (void)
{
	int			i;
	cheatvar_t	*var;

	// find all the cvars if we haven't done it yet
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get (cheatvars[numcheatvars].name,
					va("%g", cheatvars[numcheatvars].value), 0);
			cheatvars[numcheatvars].var->changed = _cheatcvar_changed;
			numcheatvars++;
		}
	}

	if (cls.state == ca_disconnected || cl.attractloop || Com_ServerState() == ss_demo || cl.maxclients == 1)
		return;		// single player can cheat

	// make sure they are all set to the proper values
	for (i=0, var = cheatvars ; i<numcheatvars ; i++, var++)
	{
		if (var->var->value != var->value)
		{
			Cvar_SetValue (var->name, var->setval);
		}
	}
}

//============================================================================



/*
==================
CL_Frame

==================
*/

#ifdef _WIN32
extern qboolean	ActiveApp;
#endif

extern cvar_t *cl_lents;
void LE_RunLocalEnts (void);

//CL_RefreshInputs
//jec - updates all input events

void IN_Commands (void);

void CL_RefreshCmd (void);
void CL_RefreshInputs (void)
{
	// process new key events
	Sys_SendKeyEvents ();

#if (defined JOYSTICK) || (defined LINUX)
	// process mice & joystick events
	IN_Commands ();
#endif

	// process console commands
	Cbuf_Execute ();

	// process packets from server
	CL_ReadPackets();

	//jec - update usercmd state
	if (cls.state > ca_connecting)
		CL_RefreshCmd();
}

void CL_LoadDeferredModels (void)
{
	if (deferred_model_index == MAX_MODELS || !cl.refresh_prepped)
		return;

	for (;;)
	{
		deferred_model_index ++;

		if (deferred_model_index == MAX_MODELS)
		{
			re.EndRegistration ();
			Com_DPrintf ("CL_LoadDeferredModels: All done.\n");
			return;
		}

		if (!cl.configstrings[CS_MODELS+deferred_model_index][0])
			continue;

		if (cl.configstrings[CS_MODELS+deferred_model_index][0] != '#')
		{
			//Com_DPrintf ("CL_LoadDeferredModels: Now loading '%s'...\n", cl.configstrings[CS_MODELS+deferred_model_index]);
			cl.model_draw[deferred_model_index] = re.RegisterModel (cl.configstrings[CS_MODELS+deferred_model_index]);
			if (cl.configstrings[CS_MODELS+deferred_model_index][0] == '*')
				cl.model_clip[deferred_model_index] = CM_InlineModel (cl.configstrings[CS_MODELS+deferred_model_index]);
			else
				cl.model_clip[deferred_model_index] = NULL;
		}

		break;
	}
}

void CL_SendCommand_Synchronous (void)
{
	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
#ifdef JOYSTICK
	IN_Commands ();
#endif

	// process console commands
	Cbuf_Execute ();

	// fix any cheating cvars
	//CL_FixCvarCheats ();

	// send intentions now
	CL_SendCmd_Synchronous ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}

void CL_Synchronous_Frame (int msec)
{
	static int	extratime;

	if (dedicated->value)
		return;

	extratime += msec;

	if (!cl_timedemo->value)
	{
		if (cls.state == ca_connected)
		{
			if (extratime < 100 && !send_packet_now)
				return;			// don't flood packets out while connecting
		}
		else
		{
			if (extratime < 1000/cl_maxfps->value)
				return;
		}
	}

	// let the mouse activate or deactivate
	IN_Frame ();

	// decide the simulation time
	cls.frametime = extratime/1000.0f;
	cl.time += extratime;
	cls.realtime = curtime;

	extratime = 0;
#if 0
	if (cls.frametime > (1.0f / cl_minfps->value))
		cls.frametime = (1.0f / cl_minfps->value);
#else
	if (cls.frametime > (1.0f / 5))
		cls.frametime = (1.0f / 5);
#endif

	// if in the debugger last frame, don't timeout
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds ();

	send_packet_now = false;

#ifdef USE_CURL
	CL_RunHTTPDownloads ();
#endif

	// fetch results from server
	CL_ReadPackets ();

	// send a new command message to the server
	CL_SendCommand_Synchronous ();

	// predict all unacknowledged movements
	CL_PredictMovement ();

	// allow rendering DLL change
	if (vid_ref->modified)
	{
		VID_ReloadRefresh ();
	}

	if (!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh ();

	CL_LoadDeferredModels();

	// update the screen
	SCR_UpdateScreen ();

	// update audio
	S_Update (cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);
	
#ifdef CD_AUDIO
	CDAudio_Update();
#endif

	if (cls.spamTime && cls.spamTime < cls.realtime)
	{
		Cbuf_AddText ("say \"" PRODUCTNAME " " VERSION " " __TIMESTAMP__ " " CPUSTRING " " BUILDSTRING " [http://r1ch.net/r1q2]\"\n");
		cls.lastSpamTime = cls.realtime;
		cls.spamTime = 0;
	}

	if (cl_lents->intvalue)
		LE_RunLocalEnts ();

	// advance local effects for next frame
	CL_RunDLights ();
	CL_RunLightStyles ();

#ifdef CINEMATICS
	SCR_RunCinematic ();
#endif

	SCR_RunConsole ();
}


//CL_SendCommand
//jec - prepare and send out the current usercmd state.
void CL_SendCommand (void)
{
	// fix any cheating cvars
	//CL_FixCvarCheats ();

	// send client packet to server
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}

void CL_Frame (int msec)
{
	//jec - this function's been rearranged a lot,
	//	to decouple various client activities.
	static int	packet_delta,
				render_delta,
				misc_delta = 1000;

	//static int inputCount = 0;

	qboolean	packet_frame=true,
				render_frame=true,
				misc_frame=true;

#ifndef NO_SERVER
	if (dedicated->intvalue)
		return;
#endif

#ifdef _DEBUG
	//if (!ActiveApp)
		//NET_Client_Sleep (50);

	if (dbg_framesleep->intvalue)
		Sys_Sleep (dbg_framesleep->intvalue);
#else
#ifdef _WIN32
	if (!ActiveApp && !Com_ServerState())
		NET_Client_Sleep (100);
#endif
#endif

	if (cl_async->intvalue != 1)
	{
		CL_Synchronous_Frame (msec);
		return;
	}

	//jec - set internal counters
	packet_delta += msec;
	render_delta += msec;
	misc_delta += msec;

	//jec - set the frame counters
	cl.time += msec;
	cls.frametime = packet_delta/1000.0f;
	cls.realtime = curtime;

	//if (cls.frametime > 0.05)
	//	Com_Printf ("Hitch warning: %f (%d ms)\n", LOG_CLIENT, cls.frametime, msec);

	//if in the debugger last frame, don't timeout
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds ();

	// don't extrapolate too far ahead
	if (cls.frametime > .5)
		cls.frametime = .5;

	//jec - determine what all should be done...
	if (!cl_timedemo->intvalue)
	{
		// packet transmission rate is too high
		if (packet_delta < 1000/cl_maxfps->intvalue)
			packet_frame = false;
	
		// don't need to do this stuff much.
		if( misc_delta < 250)
			misc_frame = false;

		// framerate is too high
		if (render_delta < 1000/r_maxfps->intvalue)
			render_frame = false;
	}

	// don't flood packets out while connecting
	if (cls.state == ca_connected)
	{
		if (packet_delta < 100)
			packet_frame = false;

#ifdef USE_CURL
		//we run full speed when connecting
		CL_RunHTTPDownloads ();
#endif
	}

	//jec - update the inputs (keybd, mouse, server, etc)
	CL_RefreshInputs ();

	if ((send_packet_now && cl_instantpacket->intvalue) || userinfo_modified)
	{
		if (cls.state < ca_connected)
		{
			userinfo_modified = false;
		}
		else
		{
			packet_frame = true;
		}
	}

	send_packet_now = false;

	//jec- send commands to the server
	//if (++inputCount >= cl_snaps->value && packet_frame)
	if (packet_frame)
	{
		packet_delta = 0;
		CL_SendCommand ();
		CL_LoadDeferredModels();

#ifdef USE_CURL
		//we run less often in game
		CL_RunHTTPDownloads ();
#endif
	}

	//jec- Render the display
	if(render_frame)
	{
		render_delta = 0;

		if(misc_frame)
		{
			//static qboolean snapsInitialized = false;

			misc_delta = 0;

			if (cls.spamTime && cls.spamTime < cls.realtime)
			{
				char buff[256];
				Com_sprintf (buff, sizeof(buff), "say \"R1Q2 %s %s %s %s [http://r1ch.net/r1q2]\"\n", VERSION,
					__TIMESTAMP__, CPUSTRING, BUILDSTRING
				);
				Cbuf_AddText (buff);
				cls.lastSpamTime = cls.realtime;
				cls.spamTime = 0;
			}
		
			//set the mouse on/off
			IN_Frame();

			if (vid_ref->modified)
			{
				VID_ReloadRefresh ();
			}
		}
		
		if (!cl.refresh_prepped && cls.state == ca_active)
			CL_PrepRefresh ();

		// predict all unacknowledged movements
		CL_PredictMovement ();

		//r1: run local ent physics/thinking/etc
		if (cl_lents->intvalue)
			LE_RunLocalEnts ();

		// update the screen
		SCR_UpdateScreen ();

		// update audio
		S_Update (cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);

	#ifdef CD_AUDIO
		if(misc_frame)
			CDAudio_Update();
	#endif

		// advance local effects for next frame
		CL_RunDLights ();
		CL_RunLightStyles ();
		SCR_RunConsole ();
	}
		//cls.framecount++;
	//}
}

//============================================================================

/*
====================
CL_Init
====================
*/
void LE_Init (void);
void CL_Init (void)
{
	int i;

#ifndef NO_SERVER
	if (dedicated->intvalue)
		return;		// nothing running on the client
#endif

	//r1: init string table
	for (i = svc_max_enttypes; i < 256; i++)
		svc_strings[i] = svc_strings[0];

	// all archived variables will now be loaded

//	Cbuf_AddText ("exec autoexec.cfg\n");
	FS_ExecConfig ("autoexec.cfg");
	Cbuf_Execute ();

	Con_Init ();	
#if defined __linux__ || defined __sgi || defined __FreeBSD__
	S_Init (true);	

	VID_Init ();
	//r1: after initing video new console size is probably in use... lets use it for remaining
	//messages
	Con_CheckResize();
#else
	VID_Init ();

	//r1: after initing video new console size is probably in use... lets use it for remaining
	//messages
	Con_CheckResize();

	S_Init (true);	// sound must be initialized after window is created
#endif
	
	V_Init ();

#ifdef CLIENT_DLL
	CL_ClDLL_Restart_f ();
#endif
	
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	M_Init ();
	
	SCR_Init ();
	//cls.disable_screen = 0;	// don't draw yet

#ifdef CD_AUDIO
	CDAudio_Init ();
#endif
	CL_InitLocal ();
	IN_Init ();

	LE_Init ();

	CL_Loc_Init ();

#ifdef USE_CURL
	CL_InitHTTPDownloads ();
#endif

	FS_ExecConfig ("postinit.cfg");
	Cbuf_Execute ();
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown(void)
{
	static qboolean isdown = false;
	
	if (isdown)
	{
		//printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

#ifdef USE_CURL
	CL_HTTP_Cleanup (true);
#endif

	CL_FreeLocs ();
	CL_WriteConfiguration (); 

#ifdef CD_AUDIO
	CDAudio_Shutdown ();
#endif
	S_Shutdown();
	IN_Shutdown ();
	VID_Shutdown();
}
