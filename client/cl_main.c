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

int deffered_model_index;

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

//cvar_t	*cl_noskins;
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
cvar_t	*cl_defertimer;
cvar_t	*cl_protocol;

cvar_t	*dbg_framesleep;
//cvar_t	*cl_snaps;

cvar_t	*cl_strafejump_hack;
cvar_t	*cl_nolerp;

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

extern	qboolean reload_video;

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
void CL_WriteDemoMessage (void)
{
	int		len, swlen;

	// the first eight bytes are just packet sequencing stuff
	/*len = net_message.cursize-8;
	swlen = LittleLong(len);
	fwrite (&swlen, 4, 1, cls.demofile);
	fwrite (net_message.data+8,	len, 1, cls.demofile);*/

	len = net_message.cursize-8;
	swlen = LittleLong(len);
	if (swlen > 0)
	{
		fwrite (&swlen, 4, 1, cls.demofile);
		fwrite (net_message.data+8, len, 1, cls.demofile);
	}
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
		Com_Printf ("Not recording a demo.\n");
		return;
	}

// finish up
	len = -1;
	fwrite (&len, 4, 1, cls.demofile);
	fclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Com_Printf ("Stopped demo.\n");
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
	byte	buf_data[MAX_MSGLEN];
	sizebuf_t	buf;
	int		i;
	int		len;
	entity_state_t	*ent;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("record <demoname>\n");
		return;
	}

	if (cls.demorecording)
	{
		Com_Printf ("Already recording.\n");
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

	if (strstr (Cmd_Argv(1), "..") || strstr (Cmd_Argv(1), "/") || strstr (Cmd_Argv(1), "\\") )
	{
		Com_Printf ("Illegal filename.\n");
		return;
	}

	//
	// open the demo file
	//
	Com_sprintf (name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	FS_CreatePath (name);
	cls.demofile = fopen (name, "wb");
	if (!cls.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
		Com_Printf ("WARNING: demos recorded at cl_protocol %d may not be compatible with non-R1Q2 clients!\n", ENHANCED_PROTOCOL_VERSION);

	Com_Printf ("recording to %s.\n", name);

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	//
	// write out messages to hold the startup information
	//
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// send the serverdata
	MSG_BeginWriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, ORIGINAL_PROTOCOL_VERSION);
	MSG_WriteLong (&buf, 0x10000 + cl.servercount);
	MSG_WriteByte (&buf, 1);	// demos are always attract loops
	MSG_WriteString (&buf, cl.gamedir);
	MSG_WriteShort (&buf, cl.playernum);

	MSG_WriteString (&buf, cl.configstrings[CS_NAME]);

	// configstrings
	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
	{
		if (cl.configstrings[i][0])
		{
			if (buf.cursize + strlen (cl.configstrings[i]) + 32 > buf.maxsize)
			{	// write it out
				len = LittleLong (buf.cursize);
				fwrite (&len, 4, 1, cls.demofile);
				fwrite (buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			MSG_BeginWriteByte (&buf, svc_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, cl.configstrings[i]);
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

		MSG_BeginWriteByte (&buf, svc_spawnbaseline);		
		MSG_WriteDeltaEntity (NULL, &null_entity_state, &cl_entities[i].baseline, &buf, true, true, 0, ENHANCED_PROTOCOL_VERSION);
	}

	MSG_BeginWriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, "precache\n");

	// write it to the demo file

	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	fwrite (buf.data, buf.cursize, 1, cls.demofile);

	// the rest of the demo file will be individual frames
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
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	Com_DPrintf ("Cmd_ForwardToServer: forwarding '%s'\n", cmd);

	//Com_Printf ("fwd: %s %s\n", cmd, Cmd_Args());

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
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

#ifdef WIN32
void Sys_AutoUpdate (void);
void CL_Update_f (void)
{
	if (cls.state != ca_disconnected)
		Com_Printf ("Can't update when connected.\n");
	else
		Sys_AutoUpdate ();
}
#endif

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
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	//r1: support \xxx escape sequences for testing various evil hacks
#ifdef _DEBUG
	{
		char *rew = Cmd_Args();
		char args[1024];
		char tmp[4];
		memset (args, 0, sizeof(args));
		strcpy (args, rew);
		memset (rew, 0, sizeof(args));
		strcpy (rew, args);
		while (*rew && *rew+3) {
			if (*rew == '\\' && isdigit(*(rew+1)) && isdigit(*(rew+2)) && isdigit(*(rew+3))) {
				strncpy (tmp, rew+1, 3);
				tmp[3] = 0;
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
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
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
	if (Cvar_VariableValue ("maxclients") > 1 || !Com_ServerState ())
	{
		Cvar_SetValue ("paused", 0);
		return;
	}

	Cvar_SetValue ("paused", !cl_paused->value);
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
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
void CL_Drop (qboolean skipdisconnect)
{
	if (cls.state == ca_uninitialized)
		return;
	if (cls.state == ca_disconnected)
		return;

	CL_Disconnect (skipdisconnect);

	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque
}


/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;
	int		port;

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
		Com_Printf ("Bad server address: %s\n", cls.servername);
		cls.connect_time = 0;
		return;
	}

	if (adr.port == 0)
		adr.port = ShortSwap (PORT_SERVER);

	port = Cvar_VariableValue ("qport");
	userinfo_modified = false;

	if (Com_ServerState() == ss_demo)
		cls.serverProtocol = 34;

	if (!cls.serverProtocol && cl_protocol->value)
		cls.serverProtocol = cl_protocol->value;

	if (!cls.serverProtocol)
		cls.serverProtocol = ENHANCED_PROTOCOL_VERSION;

	Com_DPrintf ("Cl_SendConnectPacket: protocol=%d, port=%d, challenge=%u\n", cls.serverProtocol, port, cls.challenge);

	//r1: only send enhanced connect string on new protocol in order to avoid confusing
	//    other engine mods which may or may not extend this.
	/*if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint (NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
			cls.serverProtocol, port, cls.challenge, Cvar_Userinfo());
	} else {
		Netchan_OutOfBandPrint (NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
			cls.serverProtocol, port, cls.challenge, Cvar_Userinfo());
	}*/
	Netchan_OutOfBandPrint (NS_CLIENT, &adr, "connect %i %i %i \"%s\"\n",
		cls.serverProtocol, port, cls.challenge, Cvar_Userinfo());
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
	if (cls.state == ca_connected) {
		Com_Printf ("reconnecting (soft)...\n");
		cls.state = ca_connected;
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");		
		send_packet_now = true;
		return;
	}

	if (*cls.servername) {
		if (cls.state >= ca_connected) {
			Com_Printf ("disconnecting\n");
			strcpy (cls.lastservername, cls.servername);
			CL_Disconnect(false);
			cls.connect_time = cls.realtime - 1500;
		} else
			cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		//Com_Printf ("reconnecting (hard)...\n");
	}
	
	if (*cls.lastservername) {
		cls.connect_time = -99999;
		cls.state = ca_connecting;
		Com_Printf ("reconnecting (hard)...\n");
		strcpy (cls.servername, cls.lastservername);
	} else {
		Com_Printf ("No server to reconnect to.\n");
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
		strncpy (cls.servername, "localhost", sizeof(cls.servername)-1);
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
//		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}

	if (adr.port == 0)
		adr.port = ShortSwap (PORT_SERVER);

	cls.connect_time = cls.realtime;	// for retransmit requests

	//_asm int 3;
	Com_Printf ("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint (NS_CLIENT, &adr, "getchallenge\n");
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: connect <server>\n");
		return;	
	}
	
	if (Com_ServerState ())
	{	// if running a local server, kill it and reissue
		SV_Shutdown ("Server has shut down", false, false);
	}
	else
	{
		CL_Disconnect (false);
	}

	server = Cmd_Argv (1);


	NET_Config (NET_CLIENT);		// allow remote

	CL_Disconnect (false);

	//attempt new protocol
	cls.state = ca_connecting;
	strncpy (cls.servername, server, sizeof(cls.servername)-1);
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
netadr_t	last_rcon_to;
void CL_Rcon_f (void)
{
	char	message[1024];

	//r1: buffer check ffs!
	if ((strlen(Cmd_Args()) + strlen(rcon_client_password->string) + 16) >= sizeof(message)) {
		Com_Printf ("Length of password + command exceeds maximum allowed length.\n");
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config (NET_CLIENT);		// allow remote

	strcat (message, "rcon ");

	if (*rcon_client_password->string)
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
		last_rcon_to = cls.netchan.remote_address;
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &last_rcon_to);
		if (last_rcon_to.port == 0)
			last_rcon_to.port = ShortSwap (PORT_SERVER);
	}
	
	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, &last_rcon_to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (&cl_entities, 0, sizeof(cl_entities));

	//r1: local ents clear
	Le_Reset ();

	//r1: reset
	cls.defer_rendering = 0;
	SZ_Clear (&cls.netchan.message);
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
	byte	final[32];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->value)
	{
		int	time;
		
		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames,
			time/1000.0, cl.timedemo_frames*1000.0 / time);
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
		CL_Stop_f ();

	// send a disconnect message to the server
	if (!skipdisconnect) {
		final[0] = clc_stringcmd;
		strcpy ((char *)final+1, "disconnect");

		Netchan_Transmit (&cls.netchan, 11, final);
		Netchan_Transmit (&cls.netchan, 11, final);
		Netchan_Transmit (&cls.netchan, 11, final);
	}

	CL_ClearState ();

	// stop download
	if (cls.download) {
		fclose(cls.download);
		cls.download = NULL;
	}

	cls.state = ca_disconnected;

	cls.servername[0] = '\0';

	//reset protocol attempt
	cls.serverProtocol = 0;

	Cvar_ForceSet ("$mapname", "");
	Cvar_ForceSet ("$game", "");

	//r1: swap games if needed
	Cvar_GetLatchedVars();
}

void CL_Disconnect_f (void)
{
	if (cls.state != ca_disconnected)
		Com_Error (ERR_DROP, "Disconnected from server");
}


/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
/*void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("packet <destination> <contents>\n");
		return;
	}

	NET_Config (NET_CLIENT);		// allow remote

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Com_Printf ("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = ShortSwap (PORT_SERVER);

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
}*/

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	SCR_BeginLoadingPlaque ();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Com_Printf ("\nChanging map...\n");
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

	Com_Printf ("%s\n", s);
	M_AddToServerList (net_from, s);
}


/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f (void)
{
	int			i;
	netadr_t	adr;
	char		name[32];
	char		*adrstring;
	//cvar_t		*noudp;
	//cvar_t		*noipx;

	NET_Config (NET_CLIENT);		// allow remote

	// send a broadcast packet
	Com_Printf ("pinging broadcast...\n");

	//noudp = Cvar_Get ("noudp", "0", CVAR_NOSET);
	//if (!noudp->value)
	//{
	adr.type = NA_BROADCAST;
	adr.port = ShortSwap(PORT_SERVER);
	//Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", ENHANCED_PROTOCOL_VERSION));

	//r1: only ping original, r1q2 servers respond to either, but 3.20 server would
	//reply with errors on receiving enhanced info
	Netchan_OutOfBandPrint (NS_CLIENT, &adr, va("info %i\n", ORIGINAL_PROTOCOL_VERSION));
	//}

	/*noipx = Cvar_Get ("noipx", "0", CVAR_NOSET);
	if (!noipx->value)
	{
		adr.type = NA_BROADCAST_IPX;
		adr.port = ShortSwap(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}*/

	// send a packet to each address book entry
	for (i=0 ; i<16 ; i++)
	{
		Com_sprintf (name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString (name);
		if (!adrstring || !adrstring[0])
			continue;

		Com_Printf ("pinging %s...\n", adrstring);
		if (!NET_StringToAdr (adrstring, &adr))
		{
			Com_Printf ("Bad address: %s\n", adrstring);
			continue;
		}
		if (!adr.port)
			adr.port = ShortSwap(PORT_SERVER);
		//Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", ENHANCED_PROTOCOL_VERSION));
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, va("info %i\n", ORIGINAL_PROTOCOL_VERSION));
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

	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		if (developer->value)
			Com_Printf ("client %i: %s\n", i, cl.configstrings[CS_PLAYERSKINS+i]); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
	}
	Com_Printf ("Precached all skins.\n");
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
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

	//r1: server responding to a status broadcast (ignores security check due
	//to broadcasts responding)
	if (!strcmp(c, "info") && cls.state == ca_disconnected)
	{
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
			Com_Printf ("Received %s from %s -- beginning connection.\n", c, NET_AdrToString (&net_from));
			strcpy (cls.servername, NET_AdrToString (&net_from));
			cls.state = ca_connecting;
			cls.connect_time = -99999;
			cls.passivemode = false;
		}
		return;
	}

	//r1: security check. (only allow from current connected server
	//and last destination client sent an rcon to)
	if (!NET_CompareBaseAdr (&net_from, remote) && !NET_CompareBaseAdr (&net_from, &last_rcon_to)) {
		Com_DPrintf ("Illegal %s from %s.  Ignored.\n", c, NET_AdrToString (&net_from));
		return;
	}

	Com_Printf ("%s: %s\n", NET_AdrToString (&net_from), c);

	// server connection
	if (!strcmp(c, "client_connect"))
	{
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

		Netchan_Setup (NS_CLIENT, &cls.netchan, &net_from, cls.serverProtocol, cls.quakePort);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
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
		//netadr_t remote;
		//NET_StringToAdr (cls.servername, &remote);
		/*if (cls.state < ca_connecting) {
			Com_DPrintf ("Print packet when not connected from %s.  Ignored.\n", NET_AdrToString(net_from));
			return;
		}
		if (!NET_CompareBaseAdr (net_from, remote)) {
			Com_DPrintf ("Print packet from non-server %s.  Ignored.\n", NET_AdrToString(net_from));
			return;
		}*/
		s = MSG_ReadString (&net_message);

		//BIG HACK to allow new client on old server!
		Com_Printf ("%s", s);
		if (!strstr (s, "full") &&
			!strstr (s, "locked") &&
			!strncmp (s, "Server is ", 10) &&
			cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION)
		{
			Com_Printf ("Retrying with protocol %d.\n", ORIGINAL_PROTOCOL_VERSION);
			cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
			//force immediate retry
			cls.connect_time = -99999;

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
		cls.challenge = atoi(Cmd_Argv(1));

		//r1: reset the timer so we don't send dup. getchallenges
		cls.connect_time = cls.realtime;
		CL_SendConnectPacket ();
		return;
	}

	// echo request from server
	if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, &net_from, "%s", Cmd_Argv(1) );
		return;
	}

	Com_Printf ("Unknown connectionless packet command %s\n", c);
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	int i;

	while ((i = (NET_GetPacket (NS_CLIENT, &net_from, &net_message))))
	{
		//
		// remote command packet
		//
		if (*(int *)net_message.data == -1)
		{
			if (i == -1)
				Com_Printf ("Port unreachable from %s\n", NET_AdrToString (&net_from));
			else
				CL_ConnectionlessPacket ();
			continue;
		}

		if (cls.state == ca_disconnected || cls.state == ca_connecting)
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
			Com_Error (ERR_DROP, "Connection reset by peer.");

		if (!Netchan_Process(&cls.netchan, &net_message))
			continue;		// wasn't accepted for some reason

		CL_ParseServerMessage ();

		CL_AddNetgraph ();

		if (scr_sizegraph->value)
			CL_AddSizegraph ();

		//
		// we don't know if it is ok to save a demo message until
		// after we have parsed the frame
		//
		if (cls.demorecording && !cls.demowaiting)
			CL_WriteDemoMessage ();
	}

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && cls.realtime - cls.netchan.last_received > cl_timeout->value*1000)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf ("\nServer connection timed out.\n");
			CL_Disconnect (false);
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

	if (gender_auto->value) {

		if (gender->modified) {
			// was set directly, don't override the user
			gender->modified = false;
			return;
		}

		strncpy(sk, skin->string, sizeof(sk) - 1);
		if ((p = strchr(sk, '/')) != NULL)
			*p = 0;
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
	Com_Printf ("User info settings:\n");
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
	S_Init (atoi(Cmd_Argv(1)));
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	CL_RegisterSounds ();
}

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;
int precache_model_skin;
unsigned int precache_start_time;

byte *precache_model; // used for skin checking in alias models

//#define PLAYER_MULT 23
#define	PLAYER_MULT	5

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

cl_location_t	cl_locations;

void CL_Loc_Init (void)
{
	memset (&cl_locations, 0, sizeof(cl_locations));
}

void CL_FreeLocs (void)
{
	cl_location_t	*loc = &cl_locations,
					*old = NULL;

	while (loc->next) {
		loc = loc->next;
		if (old) {
			Z_Free (old->name);
			Z_Free (old);
			old = NULL;
		}
		old = loc;
	}

	if (old) {
		Z_Free (old->name);
		Z_Free (old);
	}

	cl_locations.next = NULL;
}

void CL_LoadLoc (char *filename)
{
	cl_location_t	*loc = &cl_locations;
	char *x, *y, *z, *name, *line;
	char readLine[0xFF];
	//int len;
	FILE *fLoc;

	CL_FreeLocs ();

	FS_FOpenFile (filename, &fLoc);
	if (!fLoc) {
		Com_DPrintf ("CL_LoadLoc: %s not found\n", filename);
		return;
	}

	while ((line = (fgets (readLine, sizeof(readLine), fLoc))))
	{
		x = line;

		name = NULL;

		while (*line) {
			if (*line == ' ') {
				*line++ = '\0';
				y = line;
				break;
			}
			line++;
		}

		while (*line) {
			if (*line == ' ') {
				*line++ = '\0';
				z = line;
				break;
			}
			line++;
		}

		while (*line) {
			if (*line == ' ' && !name) {
				*line = '\0';
				name = line + 1;
			} else if (*line == '\n' || *line == '\r') {
				*line = '\0';
				break;
			}
			line++;
		}

		loc->next = Z_TagMalloc (sizeof(cl_location_t), TAGMALLOC_CLIENT_LOC);
		loc = loc->next;

		loc->name = Z_TagMalloc (strlen(name)+1, TAGMALLOC_CLIENT_LOC);
		strcpy (loc->name, name);
		VectorSet (loc->location, (float)atoi(x)/8.0, (float)atoi(y)/8.0, (float)atoi(z)/8.0);

		Com_DPrintf ("CL_AddLoc: adding location '%s'\n", name);
	}

	FS_FCloseFile (fLoc);
}

char *CL_Loc_Get (void)
{
	vec3_t			distance;
	unsigned int	length, bestlength = 0xFFFFFFFF;
	cl_location_t	*loc = &cl_locations, *best;

	while (loc->next) {
		loc = loc->next;
		VectorSubtract (loc->location, cl.refdef.vieworg, distance);
		length = VectorLength (distance);
		if (length < bestlength) {
			best = loc;
			bestlength = length;
		}
	}

	return best->name;
}

void CL_Say_Preprocessor (void)
{
	char *location_name, *p;
	char *say_text;
	
	say_text = p = Cmd_Args();

	if (cl_locations.next)
	{
		while (*say_text && *(say_text + 1))
		{
			if (*say_text == '@' && *(say_text + 1) == 'l')
			{
				int location_len, cmd_len;

				location_name = CL_Loc_Get ();

				cmd_len = strlen(Cmd_Args());
				location_len = strlen(location_name);

				if (cmd_len + location_len >= 1024)
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
}

char *colortext(char *text){
	static char ctext[2][80];
	static int c=0;
	char *p;
	char *cp;
	c^=1;
	if (!*text)
		return text;

	for (p=text, cp=ctext[c];*p!=0;p++, cp++){
		*cp=*p|128;
	}
	*cp=0;
	return ctext[c];
}

void CL_RequestNextDownload (void)
{
	//unsigned	map_checksum;		// for detecting cheater maps
	char fn[MAX_OSPATH];
	dmdl_t *pheader;

	if (cls.state != ca_connected)
		return;

	if (!allow_download->value && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

//ZOID
	if (precache_check == CS_MODELS) { // confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->value)
			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
				return; // started a download
	}
	if (precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS) {
		if (allow_download_models->value) {
			while (precache_check < CS_MODELS+MAX_MODELS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*' ||
					cl.configstrings[precache_check][0] == '#') {
					precache_check++;
					continue;
				}
				if (precache_model_skin == 0) {
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check])) {
						precache_model_skin = 1;
						return; // started a download
					}
					precache_model_skin = 1;
				}

				// checking for skins in the model
				if (!precache_model) {

					FS_LoadFile (cl.configstrings[precache_check], (void **)&precache_model);
					if (!precache_model) {
						precache_model_skin = 0;
						precache_check++;
						continue; // couldn't load it
					}
					if (LittleLong(*(unsigned *)precache_model) != IDALIASHEADER) {
						// not an alias model
						FS_FreeFile(precache_model);
						precache_model = 0;
						precache_model_skin = 0;
						precache_check++;
						continue;
					}
					pheader = (dmdl_t *)precache_model;
					if (LittleLong (pheader->version) != ALIAS_VERSION) {
						precache_check++;
						precache_model_skin = 0;
						continue; // couldn't load it
					}
				}

				pheader = (dmdl_t *)precache_model;

				while (precache_model_skin - 1 < LittleLong(pheader->num_skins)) {
					if (!CL_CheckOrDownloadFile((char *)precache_model +
						LittleLong(pheader->ofs_skins) + 
						(precache_model_skin - 1)*MAX_SKINNAME)) {
						precache_model_skin++;
						return; // started a download
					}
					precache_model_skin++;
				}
				if (precache_model) { 
					FS_FreeFile(precache_model);
					precache_model = 0;
				}
				precache_model_skin = 0;
				precache_check++;
			}
		}
		precache_check = CS_SOUNDS;
	}
	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS) { 
		if (allow_download_sounds->value) {
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank
			while (precache_check < CS_SOUNDS+MAX_SOUNDS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*') {
					precache_check++;
					continue;
				}
				Com_sprintf(fn, sizeof(fn), "sound/%s", cl.configstrings[precache_check++]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = CS_IMAGES;
	}
	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES+MAX_IMAGES) {
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank
		while (precache_check < CS_IMAGES+MAX_IMAGES &&
			cl.configstrings[precache_check][0]) {
			Com_sprintf(fn, sizeof(fn), "pics/%s.png", cl.configstrings[precache_check]);
			if (FS_LoadFile (fn, NULL) == -1) {
				Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check]);
				if (FS_LoadFile (fn, NULL) == -1) {
					Com_sprintf(fn, sizeof(fn), "pics/%s.png", cl.configstrings[precache_check]);
					//__asm int 3;
					if (!cl.configstrings_download_attempted[precache_check] && !CL_CheckOrDownloadFile(fn)) {
						//precache_check++;
						cl.configstrings_download_attempted[precache_check] = true;
						return; // started a download
					} else {
						Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check]);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check++;
							return; // started a download
						}
					}
				}
			}
			precache_check++;
		}
		precache_check = CS_PLAYERSKINS;
	}
	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT) {
		if (allow_download_players->value) {
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT) {
				int i, n;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS)%PLAYER_MULT;

				if (!cl.configstrings[CS_PLAYERSKINS+i][0]) {
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS+i], '\\')) != NULL)
					p++;
				else
					p = cl.configstrings[CS_PLAYERSKINS+i];
				strcpy(model, p);
				p = strchr(model, '/');
				if (!p)
					p = strchr(model, '\\');
				if (p) {
					*p++ = 0;
					strcpy(skin, p);
				} else
					*skin = 0;

				switch (n) {
				case 0: // model
					Com_sprintf(fn, sizeof(fn), "players/%s/tris.md2", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 1: // weapon model
					Com_sprintf(fn, sizeof(fn), "players/%s/weapon.md2", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 2: // weapon skin
					Com_sprintf(fn, sizeof(fn), "players/%s/weapon.pcx", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 3: // skin
					Com_sprintf(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 4: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				/*case 5: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/death1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 6;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 6: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/death2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 7;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 7: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/death3.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 8;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 8: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/death4.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 9;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 9: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain100_1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 10;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 10: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain100_2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 11;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 11: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain75_1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 12;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 12: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain75_2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 13;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 13: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain50_1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 14;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 14: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/jump1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 15;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 15: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain50_2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 16;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 16: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain25_1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 17;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 17: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/pain25_2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 18;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 18: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/fall1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 19;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 19: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/fall2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 20;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 20: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/gurp1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 21;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 21: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/gurp2.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 22;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				case 22: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/drown1.wav", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 23;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;*/
				}
			}
		}
		// precache phase completed
		precache_check = ENV_CNT;
	}

	if (precache_check == ENV_CNT) {
		char locbuff[MAX_QPATH];
		precache_check = ENV_CNT + 1;

		COM_StripExtension (cl.configstrings[CS_MODELS+1], locbuff);
		strcat (locbuff, ".loc");

		CL_LoadLoc (locbuff);

		/*CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);

		if (map_checksum && map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM])) {
			Com_Error (ERR_DROP, "Local map version differs from server: 0x%.8x != 0x%.8x\n",
				map_checksum, atoi(cl.configstrings[CS_MAPCHECKSUM]));
			return;
		}*/
	}

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT) {
		if (allow_download->value && allow_download_maps->value) {
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

		if (allow_download->value && allow_download_maps->value) {
			while (precache_tex < numtexinfo) {
				//char fn[MAX_OSPATH];

				Com_sprintf(fn, sizeof(fn), "textures/%s.wal", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT+999;
	}

//ZOID
	CL_RegisterSounds ();
	CL_PrepRefresh ();

	Com_DPrintf ("Precache completed in %u msec.\n", Sys_Milliseconds() - precache_start_time);

	if (cl_maxfps->value > 100)
		Com_Printf ("\n%s\nR1Q2 separates network and rendering so a cl_maxfps value > 30\nis rarely needed. Use r_maxfps to control maximum rendered FPS\n\n", colortext ("WARNING: A cl_maxfps value of over 100 is strongly discouraged"));

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("begin %i\n", precache_spawncount) );
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
		unsigned	map_checksum;		// for detecting cheater maps
		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);
		CL_RegisterSounds ();
		CL_PrepRefresh ();
		return;
	}

	precache_start_time = Sys_Milliseconds();

	precache_check = CS_MODELS;
	precache_spawncount = atoi(Cmd_Argv(1));
	precache_model = 0;
	precache_model_skin = 0;

	CL_RequestNextDownload();
}

void CL_SendStatusPacket_f (void)
{
	if (cls.state <= ca_connected)
		return;

	Netchan_OutOfBandPrint (NS_CLIENT, &cls.netchan.remote_address, "info %d\n", cls.serverProtocol);
}

void CL_Toggle_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf ("Usage: toggle cvar [option1|option2|option3|optionx]\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		cvar_t *tvar = Cvar_FindVar (Cmd_Argv(1));
		if (!tvar) {
			Com_Printf ("no such variable %s\n", Cmd_Argv(1));
			return;
		}
		if (tvar->value != 0 && tvar->value != 1) {
			Com_Printf ("not a binary variable\n");
			return;
		}
		Cvar_SetValue (Cmd_Argv(1), (int)tvar->value ^ 1);
	} else {
		Com_Printf ("XXX: fixme\n");
	}
}

void version_update (cvar_t *self, char *old, char *newValue)
{
	Cvar_Set ("cl_version", va("R1Q2 %s; %s", VERSION, *newValue ? newValue : "unknown renderer"));
}

/*void CL_SnapsMessage (cvar_t *self, char *old, char *newValue)
{
	if (self->value != (int)self->value)
	{
		Cvar_SetValue ("cl_snaps", (int)self->value);
		return;
	}

	if (self->value > 20)
	{
		Com_Printf ("cl_snaps: range is from 1-20\n");
		Cvar_ForceSet ("cl_snaps", "20");
	}
	else if (self->value < 1)
	{
		Com_Printf ("cl_snaps: range is from 1-20\n");
		Cvar_ForceSet ("cl_snaps", "1");
	}
}*/

void _name_changed (cvar_t *var, char *oldValue, char *newValue)
{
	if (strlen(newValue) > 16)
	{
		newValue[16] = 0;
		Cvar_Set ("name", newValue);
	}
	else if (!*newValue)
	{
		Cvar_Set ("name", "unnamed");
	}
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	char *glVersion;

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
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	//cl_noskins = Cvar_Get ("cl_noskins", "0", CVAR_NOSET);
//	cl_autoskins = Cvar_Get ("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get ("cl_predict", "1", 0);
	cl_backlerp = Cvar_Get ("cl_backlerp", "1", 0);
//	cl_minfps = Cvar_Get ("cl_minfps", "5", 0);
	r_maxfps = Cvar_Get ("r_maxfps", "1000", 0);
	cl_maxfps = Cvar_Get ("cl_maxfps", "30", CVAR_ARCHIVE);
	cl_async = Cvar_Get ("cl_async", "1", 0);

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

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

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
	cl_protocol = Cvar_Get ("cl_protocol", "35", 0);
#else
	cl_protocol = Cvar_Get ("cl_protocol", "35", 0);
#endif

	cl_defertimer = Cvar_Get ("cl_defertimer", "1", 0);
	cl_smoothsteps = Cvar_Get ("cl_smoothsteps", "1", 0);
	cl_instantpacket = Cvar_Get ("cl_instantpacket", "1", 0);
	cl_strafejump_hack = Cvar_Get ("cl_strafejump_hack", "0", 0);
	cl_nolerp = Cvar_Get ("cl_nolerp", "0", 0);

#ifdef NO_SERVER
	allow_download = Cvar_Get ("allow_download", "0", CVAR_ARCHIVE);
	allow_download_players  = Cvar_Get ("allow_download_players", "0", CVAR_ARCHIVE);
	allow_download_models = Cvar_Get ("allow_download_models", "1", CVAR_ARCHIVE);
	allow_download_sounds = Cvar_Get ("allow_download_sounds", "1", CVAR_ARCHIVE);
	allow_download_maps	  = Cvar_Get ("allow_download_maps", "1", CVAR_ARCHIVE);
#endif

	//haxx
	glVersion = Cvar_VariableString ("cl_version");
	(Cvar_ForceSet ("cl_version", va("R1Q2 %s; %s", VERSION, *glVersion ? glVersion : "unknown renderer" )))->changed = version_update;

	name->changed = _name_changed;

#ifdef _DEBUG
	dbg_framesleep = Cvar_Get ("dbg_framesleep", "0", 0);
#endif

	//cl_snaps = Cvar_Get ("cl_snaps", "1", 0);
	//cl_snaps->modified = false;
	//cl_snaps->changed = CL_SnapsMessage;

	//
	// register our commands
	//
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);
	Cmd_AddCommand ("skins", CL_Skins_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);

#ifdef CLIENT_DLL
	Cmd_AddCommand ("cl_restart", CL_ClDLL_Restart_f);
#endif

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

	//r1: allow passive connects
	Cmd_AddCommand ("passive", CL_Passive_f);

	//r1: toggler cvar
	Cmd_AddCommand ("toggle", CL_Toggle_f);

	//r1: server status (connectionless)
	Cmd_AddCommand ("sstatus", CL_SendStatusPacket_f);

	//r1: deprecated
#ifdef WIN32
	Cmd_AddCommand ("update", CL_Update_f);
#endif
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
	Cmd_AddCommand ("say", CL_Say_Preprocessor);
	Cmd_AddCommand ("say_team", CL_Say_Preprocessor);

	Cmd_AddCommand ("info", NULL);
	Cmd_AddCommand ("prog", NULL);
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
		Com_Printf ("Couldn't write config.cfg.\n");
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
	char	*value;
	cvar_t	*var;
} cheatvar_t;

cheatvar_t	cheatvars[] = {
	{"timescale", "1", NULL},
	{"timedemo", "0", NULL},
	{"r_drawworld", "1", NULL},
	//{"cl_testlights", "0"},
	{"r_fullbright", "0", NULL},
	{"r_drawflat", "0", NULL},
	{"paused", "0", NULL},
	{"fixedtime", "0", NULL},
	{"sw_draworder", "0", NULL},
	{"gl_lightmap", "0", NULL},
	{"gl_saturatelighting", "0", NULL},
	{NULL, NULL, NULL}
};

int		numcheatvars;

void CL_FixCvarCheats (void)
{
	int			i;
	cheatvar_t	*var;

	if (Com_ServerState() == ss_demo || (!strcmp(cl.configstrings[CS_MAXCLIENTS], "1") 
		|| !cl.configstrings[CS_MAXCLIENTS][0] ))
		return;		// single player can cheat

	// find all the cvars if we haven't done it yet
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get (cheatvars[numcheatvars].name,
					cheatvars[numcheatvars].value, 0);
			numcheatvars++;
		}
	}

	// make sure they are all set to the proper values
	for (i=0, var = cheatvars ; i<numcheatvars ; i++, var++)
	{
		if ( strcmp (var->var->string, var->value) )
		{
			Cvar_Set (var->name, var->value);
		}
	}
}

//============================================================================



/*
==================
CL_Frame

==================
*/

#ifdef WIN32
extern qboolean	ActiveApp;
#endif

extern cvar_t *cl_lents;
void LE_RunLocalEnts (void);

//CL_RefreshInputs
//jec - updates all input events

void CL_RefreshCmd (void);
void CL_RefreshInputs (void)
{
	// process new key events
	Sys_SendKeyEvents ();

#ifdef JOYSTICK
	// process mice & joystick events
	IN_Commands ();
#endif

	// process console commands
	Cbuf_Execute ();

	// process packets from server
	CL_ReadPackets();

	CL_RunDownloadQueue ();

	//jec - update usercmd state
	if (cls.state > ca_connecting)
		CL_RefreshCmd();
}

void CL_LoadDeferredModels (void)
{
	if (!cl.refresh_prepped || deffered_model_index >= MAX_MODELS)
		return;

	for (;;)
	{
		deffered_model_index ++;

		if (!cl.configstrings[CS_MODELS+deffered_model_index][0])
			continue;

		if (cl.configstrings[CS_MODELS+deffered_model_index][0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS+deffered_model_index]+1,
					sizeof(cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		} 
		else
		{
			cl.model_draw[deffered_model_index] = re.RegisterModel (cl.configstrings[CS_MODELS+deffered_model_index]);
			if (cl.configstrings[CS_MODELS+deffered_model_index][0] == '*')
				cl.model_clip[deffered_model_index] = CM_InlineModel (cl.configstrings[CS_MODELS+deffered_model_index]);
			else
				cl.model_clip[deffered_model_index] = NULL;

		}

		break;
	}
}

//CL_SendCommand
//jec - prepare and send out the current usercmd state.
void CL_SendCommand (void)
{
	// fix any cheating cvars
	CL_FixCvarCheats ();

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
	if (dedicated->value)
		return;
#endif

#ifdef _DEBUG
	if (!ActiveApp)
		NET_Client_Sleep (50);

	if (dbg_framesleep->value)
		Sys_Sleep (dbg_framesleep->value);
#else
#ifdef WIN32
	if (!ActiveApp && !Com_ServerState())
		NET_Client_Sleep (100);
#endif
#endif
	//jec - set internal counters
	packet_delta += msec;
	render_delta += msec;
	misc_delta += msec;

	//jec - set the frame counters
	cl.time += msec;
	cls.frametime = packet_delta/1000.0;
	cls.realtime = curtime;

	//if (cls.frametime > 0.05)
	//	Com_Printf ("Hitch warning: %f (%d ms)\n", cls.frametime, msec);

	//if in the debugger last frame, don't timeout
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds ();

	// don't extrapolate too far ahead
	if (cls.frametime > .5)
		cls.frametime = .5;

	//jec - determine what all should be done...
	if (!cl_timedemo->value)
	{
		// packet transmission rate is too high
		if (packet_delta < 1000/cl_maxfps->value)
			packet_frame = false;
	
		// don't need to do this stuff much.
		if( misc_delta < 250)
			misc_frame = false;

		// framerate is too high
		if (render_delta < 1000/r_maxfps->value)
			render_frame = false;
	}

	if (!cl_async->value)
		packet_frame = true;

	// don't flood packets out while connecting
	if (cls.state == ca_connected && packet_delta < 100)
		packet_frame = false;

	//jec - update the inputs (keybd, mouse, server, etc)
	CL_RefreshInputs ();

	if ((send_packet_now && cl_instantpacket->value) || userinfo_modified)
	{
		Com_DPrintf ("*** instantpacket\n");
		packet_frame = true;
	}

	send_packet_now = false;

	//jec- send commands to the server
	//if (++inputCount >= cl_snaps->value && packet_frame)
	if (packet_frame)
	{
		packet_delta = 0;
		//inputCount = 0;
		CL_SendCommand ();
		CL_LoadDeferredModels();
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

			if (reload_video)
			{
				VID_ReloadRefresh ();
				reload_video = false;
			}

			//r1ch: dynamically set value of cl_snaps to get as close as possible to 30 pps
			/*if (!snapsInitialized && cls.state == ca_active && (cls.defer_rendering + 1000) < cls.realtime)
			{
				static int msecSamples = 0;
				static float averageMsec = 0;

				if (cl_snaps->modified)
				{
					snapsInitialized = true;
				}
				else
				{
					averageMsec += msec;
					msecSamples++;

					if (msecSamples == 6)
					{
						snapsInitialized = true;
						averageMsec /= 6.0;

						msecSamples = (int)(((1000.0 / (float)averageMsec) / 30.0) + 0.5);

						if (msecSamples > 20)
							msecSamples = 20;
						else if (msecSamples < 1)
							msecSamples = 1;

						Cvar_SetValue ("cl_snaps", msecSamples);

 						Com_DPrintf ("CL_Frame: Initialized snaps to %d, averageMsec = %.2f.\n", (int)cl_snaps->value, averageMsec);
					}
				}
			}*/
		
			// check for display changes
			//VID_CheckChanges ();
		}
		
		if (!cl.refresh_prepped && cls.state == ca_active)
			CL_PrepRefresh ();

		// predict all unacknowledged movements
		CL_PredictMovement ();

		//r1: run local ent physics/thinking/etc
		if (cl_lents->value)
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
#ifndef NO_SERVER
	if (dedicated->value)
		return;		// nothing running on the client
#endif

	// all archived variables will now be loaded

	Con_Init ();	
#if defined __linux__ || defined __sgi
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
	
	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	M_Init ();	
	
	SCR_Init ();
	cls.disable_screen = true;	// don't draw yet

#ifdef CD_AUDIO
	CDAudio_Init ();
#endif
	CL_InitLocal ();
	IN_Init ();

	LE_Init ();

	CL_Loc_Init ();

//	Cbuf_AddText ("exec autoexec.cfg\n");
	FS_ExecAutoexec ();
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
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

	CL_FreeLocs ();
	CL_WriteConfiguration (); 

#ifdef CD_AUDIO
	CDAudio_Shutdown ();
#endif
	S_Shutdown();
	IN_Shutdown ();
	VID_Shutdown();
}


