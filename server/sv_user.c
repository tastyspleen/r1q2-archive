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
// sv_user.c -- server code for moving users

#include "server.h"

edict_t	*sv_player;

cvar_t	*sv_max_download_size;
cvar_t	*sv_downloadwait;

char	svConnectStuffString[1100] = {0};
char	svBeginStuffString[1100] = {0};

/*
============================================================

USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

static void SV_BaselinesMessage (qboolean userCmd);

/*
==================
SV_BeginDemoServer
==================
*/
static void SV_BeginDemoserver (void)
{
	char		name[MAX_OSPATH];

	Com_sprintf (name, sizeof(name), "demos/%s", sv.name);
	FS_FOpenFile (name, &sv.demofile);

	if (!sv.demofile)
		Com_Error (ERR_DROP, "Couldn't open %s\n", name);
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline (client_t *cl)
{
	edict_t			*svent;
	int				entnum;	

	memset (cl->lastlines, 0, sizeof(entity_state_t) * MAX_EDICTS);

	for (entnum = 1; entnum < ge->num_edicts ; entnum++)
	{
		svent = EDICT_NUM(entnum);
		if (!svent->inuse)
			continue;
		if (!svent->s.modelindex && !svent->s.sound && !svent->s.effects)
			continue;
		svent->s.number = entnum;

		//
		// take current state as baseline
		//
		VectorCopy (svent->s.origin, svent->s.old_origin);
		cl->lastlines[entnum] = svent->s;
	}
}

/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
static void SV_New_f (void)
{
	char		*gamedir;
	int			playernum;
	edict_t		*ent;
	cvarban_t	*bans;

	Com_DPrintf ("New() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("New not valid -- already spawned\n");
		return;
	}

	// demo servers just dump the file message
	if (sv.state == ss_demo)
	{
		SV_BeginDemoserver ();
		return;
	}

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
	gamedir = Cvar_VariableString ("gamedir");

	// send the serverdata
	MSG_BeginWriteByte (&sv_client->netchan.message, svc_serverdata);

	//r1: report back the same protocol they used in their connection
	MSG_WriteLong (&sv_client->netchan.message, sv_client->protocol);
	MSG_WriteLong (&sv_client->netchan.message, svs.spawncount);
	MSG_WriteByte (&sv_client->netchan.message, sv.attractloop);
	MSG_WriteString (&sv_client->netchan.message, gamedir);

	if (sv.state == ss_cinematic || sv.state == ss_pic)
		playernum = -1;
	else
		playernum = sv_client - svs.clients;
	MSG_WriteShort (&sv_client->netchan.message, playernum);

	// send full levelname
	MSG_WriteString (&sv_client->netchan.message, sv.configstrings[CS_NAME]);

	if (sv_client->protocol == ENHANCED_PROTOCOL_VERSION) {
		MSG_WriteShort (&sv_client->netchan.message, (unsigned short)sv_downloadport->intvalue);
	}

	//r1: fix for old clients that don't respond to stufftext due to pending cmd buffer
	MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message, "\n");

	MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
	MSG_WriteString (&sv_client->netchan.message, "cmd \177c version $version\n");

	bans = &cvarbans;

	while (bans->next)
	{
		bans = bans->next;
		if (strcmp (bans->cvarname, "version"))
		{
			MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
			MSG_WriteString (&sv_client->netchan.message, va("cmd \177c %s $%s\n", bans->cvarname, bans->cvarname));
		}
	}

	if (svConnectStuffString[0])
	{
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, svConnectStuffString);
	}

	//
	// game server
	// 
	if (sv.state == ss_game)
	{
		// set up the entity for the client
		ent = EDICT_NUM(playernum+1);
		ent->s.number = playernum+1;
		sv_client->edict = ent;
		memset (&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));

		//r1: per-client baselines
		SV_CreateBaseline (sv_client);

		// begin fetching configstrings
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i 0\n", svs.spawncount) );
	}

	//r1: holy shit ugly, but it works!
	//if (sv_client->state == cs_connected)
	//	Netchan_Transmit (&sv_client->netchan, 0, NULL);

}

/*
==================
SV_Configstrings_f
==================
*/
static void SV_Configstrings_f (void)
{
	int		start;
	int		len;
	Com_DPrintf ("Configstrings() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		//r1: dprintf to avoid console spam from idiot client
		Com_DPrintf ("configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Configstrings_f from %s for a different level\n", sv_client->name);
		SV_New_f ();
		return;
	}

	start = atoi(Cmd_Argv(2));

	//r1: huge security fix !! remote DoS by negative here.
	if (start < 0) {
		Com_Printf ("Illegal configstrings offset from %s[%s], client dropped\n", sv_client->name, NET_AdrToString (&sv_client->netchan.remote_address));
		Blackhole (&sv_client->netchan.remote_address, true, "attempted DoS (negative configstrings)");
		SV_DropClient (sv_client);
		return;
	}

	// write a packet full of data
	if (sv_client->protocol == ORIGINAL_PROTOCOL_VERSION)
	{
plainStrings:
		start = atoi(Cmd_Argv(2));
		while ( sv_client->netchan.message.cursize < 1200
			&& start < MAX_CONFIGSTRINGS)
		{
			if (sv.configstrings[start][0])
			{
				if (sv_client->netchan.message.cursize && strlen(sv.configstrings[start]) + sv_client->netchan.message.cursize > 1200)
					break;
				MSG_BeginWriteByte (&sv_client->netchan.message, svc_configstring);
				MSG_WriteShort (&sv_client->netchan.message, start);

				len = strlen(sv.configstrings[start]);

				//MSG_WriteString (&sv_client->netchan.message, sv.configstrings[start]);
				SZ_Write (&sv_client->netchan.message, sv.configstrings[start], len > MAX_QPATH ? MAX_QPATH : len);
				SZ_Write (&sv_client->netchan.message, "\0", 1);
			}
			start++;
		}
	}
	else
	{
		int realBytes;
		int result;
		z_stream z = {0};
		sizebuf_t zBuff;
		byte tempConfigStringPacket[MAX_USABLEMSG];
		byte compressedStringStream[MAX_USABLEMSG];

		z.next_out = compressedStringStream;
		z.avail_out = sizeof(compressedStringStream);
		realBytes = 0;

		if (deflateInit2 (&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflateInit2() failed.\n");
			goto plainStrings;
		}

		SZ_Init (&zBuff, tempConfigStringPacket, sizeof (tempConfigStringPacket));

		while ( z.total_out < (1200 - sv_client->netchan.message.cursize) )
		{
			SZ_Clear (&zBuff);
			while (start < MAX_CONFIGSTRINGS)
			{
				if (sv.configstrings[start][0])
				{
					MSG_BeginWriteByte (&zBuff, svc_configstring);
					MSG_WriteShort (&zBuff, start);

					len = strlen(sv.configstrings[start]);
					
					SZ_Write (&zBuff, sv.configstrings[start], len > MAX_QPATH ? MAX_QPATH : len);
					SZ_Write (&zBuff, "\0", 1);
					
					//MSG_WriteString (&zBuff, sv.configstrings[start]);

					if (zBuff.cursize >= 150 || z.total_out > (1000 - sv_client->netchan.message.cursize))
					{
						start++;
						break;
					}
				}
				start++;
			}

			if (!zBuff.cursize)
				break;

			z.avail_in = zBuff.cursize;
			z.next_in = zBuff.data;

			realBytes += zBuff.cursize;

			result = deflate(&z, Z_SYNC_FLUSH);
			if (result != Z_OK)
			{
				SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() Z_SYNC_FLUSH failed.\n");
				goto plainStrings;
			}
			if (z.avail_out == 0)
			{
				SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() ran out of buffer space.\n");
				goto plainStrings;
			}
		}

		result = deflate(&z, Z_FINISH);
		if (result != Z_STREAM_END) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() Z_FINISH failed.\n");
			goto plainStrings;
		}

		result = deflateEnd(&z);
		if (result != Z_OK) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflateEnd() failed.\n");
			goto plainStrings;
		}

		if (z.total_out > realBytes)
		{
			Com_DPrintf ("SV_Configstrings_f: %d bytes would be a %d byte zPacket\n", realBytes, z.total_out);
			goto plainStrings;
		}

		Com_DPrintf ("SV_Configstrings_f: wrote %d bytes in a %d byte zPacket\n", realBytes, z.total_out);

		MSG_BeginWriteByte (&sv_client->netchan.message, svc_zpacket);
		MSG_WriteShort (&sv_client->netchan.message, z.total_out);
		MSG_WriteShort (&sv_client->netchan.message, realBytes);
		SZ_Write (&sv_client->netchan.message, compressedStringStream, z.total_out);

	}
	// send next command

	if (start == MAX_CONFIGSTRINGS)
	{
		//r1: start sending them in this packet for maximum efficiency
		if (sv_client->netchan.message.cursize < 1100)
		{
			Com_DPrintf ("SV_Configstrings_f: %d bytes remaining, going to baselines\n", 1200 - sv_client->netchan.message.cursize);
			SV_BaselinesMessage (false);
		}
		else
		{
			MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
			MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i 0\n",svs.spawncount) );
		}
	}
	else
	{
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i %i\n",svs.spawncount, start) );
	}

	//r1: holy shit ugly, but it works!
	//if (sv_client->state == cs_connected)
	//	Netchan_Transmit (&sv_client->netchan, 0, NULL);
}

/*
==================
SV_Baselines_f
==================
*/
void SV_BaselinesMessage (qboolean userCmd)
{
	int				startPos;
	int				start;

	entity_state_t	*base;

	Com_DPrintf ("Baselines() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected)
	{
		Com_Printf ("%s: baselines not valid -- already spawned\n", sv_client->name);
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if (userCmd)
	{
		if ( atoi(Cmd_Argv(1)) != svs.spawncount)
		{
			Com_Printf ("SV_Baselines_f from different level\n");
			SV_New_f ();
			return;
		}

		startPos = atoi(Cmd_Argv(2));
	}
	else
	{
		startPos = 0;
	}

	//r1: huge security fix !! remote DoS by negative here.
	if (startPos < 0)
	{
		Com_Printf ("Illegal baseline offset from %s[%s], client dropped\n", sv_client->name, NET_AdrToString (&sv_client->netchan.remote_address));
		Blackhole (&sv_client->netchan.remote_address, true, "attempted DoS (negative baselines)");
		SV_DropClient (sv_client);
		return;
	}

	start = startPos;

	// write a packet full of data
	//r1: use new per-client baselines
	if (sv_client->protocol == ORIGINAL_PROTOCOL_VERSION)
	{
plainLines:
		start = startPos;
		while ( sv_client->netchan.message.cursize < 1200
			&& start < MAX_EDICTS)
		{
			base = &sv_client->lastlines[start];
			if (base->modelindex || base->sound || base->effects)
			{
				MSG_BeginWriteByte (&sv_client->netchan.message, svc_spawnbaseline);
				MSG_WriteDeltaEntity (NULL, &null_entity_state, base, &sv_client->netchan.message, true, true, false, sv_client->protocol);
			}
			start++;
		}
	}
	else
	{
		int realBytes;
		int result;
		z_stream z = {0};
		sizebuf_t zBuff;
		byte tempBaseLinePacket[MAX_USABLEMSG];
		byte compressedLineStream[MAX_USABLEMSG];

		//memset (&z, 0, sizeof(z));

		z.next_out = compressedLineStream;
		z.avail_out = sizeof(compressedLineStream);
		realBytes = 0;

		if (deflateInit2 (&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflateInit2() failed.\n");
			goto plainLines;
		}

		SZ_Init (&zBuff, tempBaseLinePacket, sizeof (tempBaseLinePacket));

		while ( z.total_out < (1200 - sv_client->netchan.message.cursize) )
		{
			SZ_Clear (&zBuff);
			while (start < MAX_EDICTS)
			{
				base = &sv_client->lastlines[start];
				if (base->modelindex || base->sound || base->effects)
				{
					MSG_BeginWriteByte (&zBuff, svc_spawnbaseline);
					MSG_WriteDeltaEntity (NULL, &null_entity_state, base, &zBuff, true, true, false, sv_client->protocol);

					if (zBuff.cursize >= 150 || z.total_out > (1000 - sv_client->netchan.message.cursize))
					{
						start++;
						break;
					}
				}
				start++;
			}

			if (!zBuff.cursize)
				break;

			z.avail_in = zBuff.cursize;
			z.next_in = zBuff.data;

			realBytes += zBuff.cursize;

			result = deflate(&z, Z_SYNC_FLUSH);
			if (result != Z_OK)
			{
				SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() Z_SYNC_FLUSH failed.\n");
				goto plainLines;
			}
			if (z.avail_out == 0)
			{
				SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() ran out of buffer space.\n");
				goto plainLines;
			}
		}

		result = deflate(&z, Z_FINISH);
		if (result != Z_STREAM_END) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() Z_FINISH failed.\n");
			goto plainLines;
		}

		result = deflateEnd(&z);
		if (result != Z_OK) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflateEnd() failed.\n");
			goto plainLines;
		}

		if (z.total_out > realBytes)
		{
			Com_DPrintf ("SV_Baselines_f: %d bytes would be a %d byte zPacket\n", realBytes, z.total_out);
			goto plainLines;
		}

		Com_DPrintf ("SV_Baselines_f: wrote %d bytes in a %d byte zPacket\n", realBytes, z.total_out);

		MSG_BeginWriteByte (&sv_client->netchan.message, svc_zpacket);
		MSG_WriteShort (&sv_client->netchan.message, z.total_out);
		MSG_WriteShort (&sv_client->netchan.message, realBytes);
		SZ_Write (&sv_client->netchan.message, compressedLineStream, z.total_out);
	}

	// send next command
	if (start == MAX_EDICTS)
	{
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("precache %i\n", svs.spawncount) );
	}
	else
	{
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i %i%s\n",svs.spawncount, start, Cmd_Argc() == 4 ? " force" : "") );
	}

	//r1: holy shit ugly, but it works!
	//if (sv_client->state == cs_connected)
	//	Netchan_Transmit (&sv_client->netchan, 0, NULL);

	if (!userCmd)
		Com_DPrintf ("SV_Baselines_f: netchan is now %d bytes.\n", sv_client->netchan.message.cursize);
}

static void SV_Baselines_f (void)
{
	SV_BaselinesMessage (true);
}

int SV_CountPlayers (void)
{
	int i;
	int count = 0;
	client_t *cl;

	if (!svs.initialized)
		return 0;

	for (i=0,cl=svs.clients; i < maxclients->intvalue ; i++,cl++)
	{
		if (cl->state != cs_spawned)
			continue;

		count++;
	}

	return count;
}

/*
==================
SV_Begin_f
==================
*/
static void SV_Begin_f (void)
{
	Com_DPrintf ("Begin() from %s\n", sv_client->name);

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Com_Printf ("SV_Begin_f from %s for a different level\n", sv_client->name);
		SV_New_f ();
		return;
	}

	//r1: they didn't respond to version probe
	if (!sv_client->versionString)
	{
		Com_Printf ("WARNING: Didn't receive 'version' string from %s, hacked client?\n", sv_client->name);
		SV_DropClient (sv_client);
		return;
	}

	sv_client->state = cs_spawned;

	//r1: check dll versions for struct mismatch
	if (sv_client->edict->client == NULL)
		Com_Error (ERR_DROP, "Tried to run API V4 game on a V3 server!!");

	if (sv_deny_q2ace->intvalue)
	{
		SV_ClientPrintf (sv_client, PRINT_CHAT, "console: p_auth q2acedetect\r                                         \rWelcome to %s! [%d/%d players, %d minutes into game]\n", hostname->string, SV_CountPlayers(), maxclients->intvalue, (int)((float)sv.time / 1000 / 60));
		//SV_ClientPrintf (sv_client, PRINT_CHAT, "p_auth                                                                                                                                                                                                                                                                                                                                \r                 \r");
	}

	if (svBeginStuffString[0])
	{
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, svBeginStuffString);
	}

	// call the game begin function
	ge->ClientBegin (sv_player);

	//give them some movement
	sv_client->commandMsec = 1800;
	sv_client->commandMsecOverflowCount = 0;

	//r1: this is in bad place
	//Cbuf_InsertFromDefer ();
}

//=============================================================================

/*void SV_Protocol_Test_f (void)
{
	SV_BroadcastPrintf (PRINT_HIGH, "%s is protocol %d\n", sv_client->name, sv_client->netchan.protocol);
}*/

/*
==================
SV_NextDownload_f
==================
*/
static void SV_NextDownload_f (void)
{
	int		r;
	int		percent;
	int		size;

	if (!sv_client->download)
		return;
	
	r = sv_client->downloadsize - sv_client->downloadcount;

	if (r > 1100)
		r = 1100;

	if (r + sv_client->datagram.cursize >= MAX_USABLEMSG)
		r = MAX_USABLEMSG - sv_client->datagram.cursize - 100;

	if (sv_client->downloadCompressed)
	{
		byte		zOut[0xFFFF];
		byte		*buff;
		z_stream	z = {0};
		int			i, j;
		int			realBytes;
		int			result;

		z.next_out = zOut;
		z.avail_out = sizeof(zOut);

		realBytes = 0;

		if (deflateInit2 (&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflateInit2() failed.\n");
			SV_DropClient (sv_client);
			return;
		}

		j = 0;

		if (r > 1000)
			r = 1000;

		if (r + sv_client->datagram.cursize >= MAX_USABLEMSG)
			r = MAX_USABLEMSG - sv_client->datagram.cursize - 400;

		//if (sv_client->downloadcount >= 871224)
		//	DEBUGBREAKPOINT;

		while ( z.total_out < r )
		{
			i = 300;

			if (sv_client->downloadcount + j + i > sv_client->downloadsize)
				i = sv_client->downloadsize - (sv_client->downloadcount + j);

			buff = sv_client->download + sv_client->downloadcount + j;

			z.avail_in = i;
			z.next_in = buff;

			realBytes += i;

			j += i;

			result = deflate(&z, Z_SYNC_FLUSH);
			if (result != Z_OK)
			{
				SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() Z_SYNC_FLUSH failed.\n");
				SV_DropClient (sv_client);
				return;
			}

			if (z.avail_out == 0)
			{
				SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() ran out of buffer space.\n");
				SV_DropClient (sv_client);
				return;
			}

			if (sv_client->downloadcount + j == sv_client->downloadsize)
				break;
		}

		result = deflate(&z, Z_FINISH);
		if (result != Z_STREAM_END) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflate() Z_FINISH failed.\n");
			SV_DropClient (sv_client);
			return;
		}

		result = deflateEnd(&z);
		if (result != Z_OK) {
			SV_ClientPrintf (sv_client, PRINT_HIGH, "deflateEnd() failed.\n");
			SV_DropClient (sv_client);
			return;
		}

		if (z.total_out >= realBytes)
			goto olddownload;

		MSG_BeginWriteByte (&sv_client->netchan.message, svc_zdownload);
		MSG_WriteShort (&sv_client->netchan.message, z.total_out);

		size = sv_client->downloadsize;

		if (!size)
			size = 1;

		sv_client->downloadcount += realBytes;
		percent = sv_client->downloadcount*100/size;
		
		MSG_WriteByte (&sv_client->netchan.message, percent);

		MSG_WriteShort (&sv_client->netchan.message, realBytes);
		SZ_Write (&sv_client->netchan.message, zOut, z.total_out);
	}
	else
	{
olddownload:
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, r);

		sv_client->downloadcount += r;
		size = sv_client->downloadsize;

		if (!size)
			size = 1;

		percent = sv_client->downloadcount*100/size;
		MSG_WriteByte (&sv_client->netchan.message, percent);

		SZ_Write (&sv_client->netchan.message,
			sv_client->download + sv_client->downloadcount - r, r);
	}

	//r1: holy shit ugly, but it works!
	//if (sv_client->state == cs_connected)
	//	Netchan_Transmit (&sv_client->netchan, 0, NULL);

	if (sv_client->downloadcount != sv_client->downloadsize)
		return;

	FS_FreeFile (sv_client->download);
	sv_client->download = NULL;
}

#define	DL_UDP	0x00000001
#define	DL_TCP	0x00000002

/*
==================
SV_BeginDownload_f
==================
*/
//char * ZLibCompressChunk(char *Chunk, int *len, int method);
static void SV_BeginDownload_f(void)
{
	char	*name;
	int offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check

	//r1: rewrite '\' to prevent 'download \/server.cfg from game root
	//no good, client doesn't accept
	//while ((p = strstr(name, "\\")))
	//	*p = '/';

	//block the really nasty ones
	if (*name == '\\' || name[strlen(name)-1] == '/' || strstr (name, ".."))
	{
		Com_Printf ("Refusing illegal download path %s to %s\n", name, sv_client->name);
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		Blackhole (&sv_client->netchan.remote_address, true, "download exploit (%s)", name);
		SV_DropClient (sv_client);
		return;
	}
	else if (*name == '.' 
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// r1: \ is bad
		|| strchr (name, '\\')
		// MUST be in a subdirectory	
		|| !strchr (name, '/'))
	{
		Com_Printf ("Refusing bad download path %s to %s\n", name, sv_client->name);
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}
	else if	(!allow_download->intvalue
			|| (strncmp(name, "players/", 8) == 0 && !(allow_download_players->intvalue & DL_UDP))
			|| (strncmp(name, "models/", 7) == 0 && !(allow_download_models->intvalue & DL_UDP))
			|| (strncmp(name, "sound/", 6) == 0 && !(allow_download_sounds->intvalue & DL_UDP))
			|| (strncmp(name, "maps/", 5) == 0 && !(allow_download_maps->intvalue & DL_UDP)))
	{
		Com_DPrintf ("Refusing to download %s to %s\n", name, sv_client->name);
		if (strncmp(name, "maps/", 5) == 0 && !(allow_download_maps->intvalue & DL_UDP))
		{
			if (sv_mapdownload_denied_message->modified)
			{
				ExpandNewLines (sv_mapdownload_denied_message->string);
				sv_mapdownload_denied_message->modified = false;
			}
			if (*sv_mapdownload_denied_message->string)
				SV_ClientPrintf (sv_client, PRINT_HIGH, "%s\n", sv_mapdownload_denied_message->string);
		}
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	if (sv_client->download)
		FS_FreeFile (sv_client->download);

	sv_client->downloadsize = FS_LoadFile (name, (void **)&sv_client->download);

	if (!sv_client->download)
	{
		Com_DPrintf ("Couldn't download %s to %s\n", name, sv_client->name);

		MSG_BeginWriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	if (sv_max_download_size->intvalue && sv_client->downloadsize > sv_max_download_size->intvalue)
	{
		SV_ClientPrintf (sv_client, PRINT_HIGH, "Server refused %s, %d bytes > %d maximum allowed for auto download.\n", name, sv_client->downloadsize, sv_max_download_size->intvalue);

		MSG_BeginWriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);

		Com_DPrintf ("Refused %s to %s because it exceeds sv_max_download_size\n", name, sv_client->name);
		return;
	}

	sv_client->downloadcount = offset;

	if (offset > sv_client->downloadsize)
		sv_client->downloadcount = sv_client->downloadsize;

	if (strncmp(name, "maps/", 5) == 0)
	{
		if (sv_mapdownload_ok_message->modified)
		{
			ExpandNewLines (sv_mapdownload_ok_message->string);
			sv_mapdownload_ok_message->modified = false;
		}
		if (*sv_mapdownload_ok_message->string)
			SV_ClientPrintf (sv_client, PRINT_HIGH, "%s\n", sv_mapdownload_ok_message->string);
	}

	//r1: r1q2 zlib udp downloads?
	if (!Q_stricmp (Cmd_Argv(3), "udp-zlib"))
		sv_client->downloadCompressed = true;
	else
		sv_client->downloadCompressed = false;

	SV_NextDownload_f ();
	Com_Printf ("UDP downloading %s to %s%s\n", name, sv_client->name, sv_client->downloadCompressed ? " with zlib" : "");
}



//============================================================================


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
static void SV_Disconnect_f (void)
{
//	SV_EndRedirect ();
	Com_Printf ("Dropping %s, client issued 'disconnect'.\n", sv_client->name);
	SV_DropClient (sv_client);	
}


/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
static void SV_ShowServerinfo_f (void)
{
	char	*s;
	char	*p;

	int		flip;

	//r1: this is a client issued command !
	//Info_Print (Cvar_Serverinfo());

	s = Cvar_Serverinfo();

	//skip beginning \\ char
	s++;

	flip = 0;
	p = s;

	//make it more readable
	while (*p)
	{
		if (*p == '\\')
		{
			if (flip)
				*p = '\n';
			else
				*p = '=';
			flip ^= 1;
		}
		p++;
	}

	SV_ClientPrintf (sv_client, PRINT_HIGH, "%s\n", s);
}

static void SV_ClientServerinfo_f (void)
{
	SV_ClientPrintf (sv_client, PRINT_HIGH, "You are running at protocol %d, this server supports protocols %d and %d. Running an API version %d game.\n", sv_client->protocol, ORIGINAL_PROTOCOL_VERSION, ENHANCED_PROTOCOL_VERSION, ge->apiversion);
}

static void SV_NoGameData_f (void)
{
	sv_client->nodata ^= 1;
}

static void CvarBanDrop (cvarban_t *cvar, banmatch_t *ban)
{
	SV_ClientPrintf (sv_client, PRINT_HIGH, "%s\n", ban->message);
	if (ban->blockmethod == CVARBAN_BLACKHOLE)
		Blackhole (&sv_client->netchan.remote_address, true, "cvarban: %s == %s", cvar->cvarname, Cmd_Args2(2));
	Com_Printf ("Dropped client %s, cvarban: %s == %s\n", sv_client->name, cvar->cvarname, Cmd_Args2(2));
	SV_DropClient (sv_client);
}

static void SV_CvarResult_f (void)
{
	cvarban_t *bans = &cvarbans;
	banmatch_t *match;
	char *result = Cmd_Args2(2);

	if (!Q_stricmp (Cmd_Argv(1), "version"))
	{
		if (sv_client->versionString)
			Z_Free (sv_client->versionString);
		sv_client->versionString = CopyString (result, TAGMALLOC_CVARBANS);
		if (dedicated->intvalue)
			Com_Printf ("%s[%s]: protocol %d: \"%s\"\n", sv_client->name, NET_AdrToString(&sv_client->netchan.remote_address), sv_client->protocol, result);
	}

	if (!*result)
		return;

	while (bans->next)
	{
		bans = bans->next;

		if (!Q_stricmp (bans->cvarname, Cmd_Argv(1)))
		{
			match = &bans->match;

			while (match->next)
			{
				match = match->next;

				if (*match->matchvalue == '*')
				{
					CvarBanDrop (bans, match);
					return;
				}

				if (*match->matchvalue+1)
				{
					float intresult, matchint;
					intresult = atof(result);
					matchint = atof(match->matchvalue+1);

					if (*match->matchvalue == '>')
					{
						if (intresult > matchint) {
							CvarBanDrop (bans, match);
							return;
						}
						continue;					
					}
					else if (*match->matchvalue == '<')
					{
						if (intresult < matchint)
						{
							CvarBanDrop (bans, match);
							return;
						}
						continue;
					}
					else if (*match->matchvalue == '=')
					{
						if (intresult == matchint)
						{
							CvarBanDrop (bans, match);
							return;
						}
						continue;
					}

					if (*match->matchvalue == '!')
					{
						char *matchstring = match->matchvalue + 1;
						if (Q_stricmp (matchstring, result))
						{
							CvarBanDrop (bans, match);
							return;
						}
						continue;
					}
					else if (*match->matchvalue == '~')
					{
						char *matchstring = match->matchvalue + 1;
						if (strstr (result, matchstring))
						{
							CvarBanDrop (bans, match);
							return;
						}
						continue;
					}
				}

				if (!Q_stricmp (match->matchvalue, result))
				{
					CvarBanDrop (bans, match);
					return;
				}
			}

			return;
		}
	}

	if (Q_stricmp (Cmd_Argv(1), "version"))
	{
		Com_Printf ("Dropping %s for bad cvar check result ('%s' unrequested).\n", sv_client->name, Cmd_Argv(1));
		SV_DropClient (sv_client);
	}
}

static void SV_Floodme_f (void)
{
	unsigned char paket[] = {
	0x0D, 0x20, 0x05, 0xAB, 0xFE, 0xCA, 0xAD, 0xAB, 0xFE, 0xCA, 0xAD, 0xAB, 0xFE, 0xCA, 0xAD,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0xD1, 0xD1, 0xD2, 0xD2, 0xD3, 0xD3, 0xD4, 0xD4, 0xD5, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD8, 0xD8,
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 
    0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 
	0xFF, 0xFF, 0xFF, 0xFF};

	MSG_WriteByte (&sv_client->netchan.message, svc_configstring);
	MSG_WriteShort (&sv_client->netchan.message, CS_PLAYERSKINS+2);
	MSG_WriteString (&sv_client->netchan.message, paket);
}

void SV_Nextserver (void)
{
	char	*v;

	//ZOID, ss_pic can be nextserver'd in coop mode
	if (sv.state == ss_game || (sv.state == ss_pic && !Cvar_VariableValue("coop")))
		return;		// can't nextserver while playing a normal game

	svs.spawncount++;	// make sure another doesn't sneak in
	v = Cvar_VariableString ("nextserver");
	if (!v[0])
		Cbuf_AddText ("killserver\n");
	else
	{
		Cbuf_AddText (v);
		Cbuf_AddText ("\n");
	}
	Cvar_Set ("nextserver","");
}

/*
==================
SV_Nextserver_f

A cinematic has completed or been aborted by a client, so move
to the next server,
==================
*/
static void SV_Nextserver_f (void)
{
	if ( atoi(Cmd_Argv(1)) != svs.spawncount ) {
		Com_DPrintf ("Nextserver() from wrong level, from %s\n", sv_client->name);
		return;		// leftover from last server
	}

	Com_DPrintf ("Nextserver() from %s\n", sv_client->name);

	SV_Nextserver ();
}

typedef struct
{
	char	/*@null@*/ *name;
	void	/*@null@*/ (*func) (void);
} ucmd_t;

static ucmd_t ucmds[] =
{
	// auto issued
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"baselines", SV_Baselines_f},
	{"begin", SV_Begin_f},

	{"nextserver", SV_Nextserver_f},

	{"disconnect", SV_Disconnect_f},

	// issued by hand at client consoles	
	{"info", SV_ShowServerinfo_f},

	{"sinfo", SV_ClientServerinfo_f},
	{"floodme", SV_Floodme_f},
	{"\177c", SV_CvarResult_f},

	{"nogamedata", SV_NoGameData_f},

	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},

	{NULL, NULL}
};

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteUserCommand (char *s)
{
	char				*teststring;

	ucmd_t				*u;
	bannedcommands_t	*x;
	nullcmd_t			*y;
	
	//r1: catch attempted server exploits
	if (strchr(s, '$'))
	{
		teststring = Cmd_MacroExpandString(s);
		if (!teststring)
			return;

		if (strcmp (teststring, s))
		{
			Blackhole (&net_from, true, "attempted command expansion: %s", s);
			SV_KickClient (sv_client, "attempted server exploit", NULL);
			return;
		}
	}

	//r1: catch end-of-message exploit
	if (strchr (s, '\xFF'))
	{
		char *ptr;
		ptr = strchr (s, '\xFF');
		ptr -= 8;
		if (ptr < s)
			ptr = s;
		Blackhole (&net_from, true, "0xFF in command packet (%.32s)", MakePrintable (ptr));
		SV_KickClient (sv_client, "attempted command exploit", NULL);
		return;
	}

	//r1: allow filter of high bit commands (eg \n\r in say cmds)
	if (sv_filter_stringcmds->intvalue)
		strcpy (s, StripHighBits(s, (int)sv_filter_stringcmds->intvalue == 2));

	Cmd_TokenizeString (s, false);
	sv_player = sv_client->edict;

	for (x = bannedcommands.next; x; x = x->next)
	{
		if (!strcmp (Cmd_Argv(0), x->name))
		{
			if (x->logmethod == CMDBAN_LOG_MESSAGE)
				Com_Printf ("SV_ExecuteUserCommand: %s tried to use '%s'\n", sv_client->name, x->name);

			if (x->kickmethod == CMDBAN_MESSAGE)
				SV_ClientPrintf (sv_client, PRINT_HIGH, "The '%s' command has been disabled by the server administrator.\n", x->name);
			else if (x->kickmethod == CMDBAN_KICK)
				SV_DropClient (sv_client);

			return;
		}
	}

	for (u=ucmds ; u->name ; u++)
	{
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			u->func ();

			//r1ch: why break?
			return;
		}
	}

	//r1: do we really want to be passing commands from unconnected players
	//to the game dll at this point? doesn't sound like a good idea to me
	//especially if the game dll does its own banning functions after connect
	//as banned players could spam game commands (eg say) whilst connecting
	if (sv_client->state < cs_spawned && !sv_allow_unconnected_cmds->intvalue)
		return;

	//r1: say parser (ick)
	if (!strcmp (Cmd_Argv(0), "say"))
	{
		//r1: nocheat kicker/spam filter
		if (!Q_strncasecmp (Cmd_Args(), "\"NoCheat V2.", 12))
		{
			if (sv_nc_kick->intvalue)
			{
				if ((int)(sv_nc_kick->intvalue) & 256)
				{
					Com_Printf ("%s was dropped for using NoCheat\n", sv_client->name);
					if (((int)sv_nc_announce->intvalue & 256) && sv_client->state == cs_spawned && *sv_client->name)
						SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: client is using NoCheat\n", sv_client->name);

					SV_ClientPrintf (sv_client, PRINT_HIGH, "NoCheat is not permitted on this server, please use regular Quake II.\n");
					SV_DropClient (sv_client);
					return;
				}
				else if (((int)(sv_nc_kick->intvalue) & 1) && strstr(Cmd_Args(), "Code\\-1\\"))
				{
					Com_Printf ("%s was dropped for failing NoCheat code check\n", sv_client->name);
					if (((int)sv_nc_announce->intvalue & 1) && sv_client->state == cs_spawned && *sv_client->name)
						SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: invalid NoCheat code\n", sv_client->name);

					SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires a valid NoCheat code. Please check you are running in OpenGL mode.\n");
					SV_DropClient (sv_client);
					return;
				}
				else if (((int)(sv_nc_kick->intvalue) & 2) && strstr(Cmd_Args(), "Video"))
				{
					Com_Printf ("%s was dropped for failing NoCheat video check\n", sv_client->name);
					if (((int)sv_nc_announce->intvalue & 2) && sv_client->state == cs_spawned && *sv_client->name)
						SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: failed NoCheat video check\n", sv_client->name);

					SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires a NoCheat approved vid_ref.\n");
					SV_DropClient (sv_client);
					return;
				}
				else if (((int)(sv_nc_kick->intvalue) & 4) && strstr(Cmd_Args(), "modelCheck"))
				{
					Com_Printf ("%s was dropped for failing NoCheat model check\n", sv_client->name);
					if (((int)sv_nc_announce->intvalue & 4) && sv_client->state == cs_spawned && *sv_client->name)
						SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: failed NoCheat model check\n", sv_client->name);

					SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires NoCheat approved models.\n");
					SV_DropClient (sv_client);
					return;
				}
				else if (((int)(sv_nc_kick->intvalue) & 8) && (strstr(Cmd_Args(), "FrkQ2") || strstr(Cmd_Args(), "Hack") || strstr(Cmd_Args(), "modelCheck") || strstr(Cmd_Args(), "glCheck")))
				{
					Com_Printf ("%s was dropped for failing additional NoCheat checks\n", sv_client->name);
					if (((int)sv_nc_announce->intvalue & 8) && sv_client->state == cs_spawned && *sv_client->name)
						SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: failed NoCheat hack checks\n", sv_client->name);

					SV_ClientPrintf (sv_client, PRINT_HIGH, "This server requires NoCheat approved video settings.\n");
					SV_DropClient (sv_client);
					return;
				}

				Com_Printf ("%s passed NoCheat verifications\n", sv_client->name);
				if (((int)sv_nc_announce->intvalue & 128) && sv_client->state == cs_spawned && *sv_client->name)
					SV_BroadcastPrintf (PRINT_HIGH, "%s passed NoCheat verifications\n", sv_client->name);
			}

			sv_client->notes |= CLIENT_NOCHEAT;

			if (sv_filter_nocheat_spam->intvalue)
				return;
		}

		//r1: anti q2ace code (for the various hacks that have turned q2ace into cheat)
		if (sv_deny_q2ace->intvalue && !strncmp (Cmd_Args(), "q2ace v", 7) && (
			strstr (Cmd_Args(), "- Authentication") ||
			strstr (Cmd_Args(), "- Failed Auth")
			))
		{
			SV_KickClient (sv_client, "client is using q2ace", "q2ace is not permitted on this server, please use regular Quake II.\n");
			return;
		}

		//r1: note, we can reset on say as it's a "known good" command. resetting on every command is bad
		//since stuff like q2admin spams tons of stuffcmds all the time...
		sv_client->idletime = 0;
	}

	for (y = nullcmds.next; y; y = y->next)
	{
		if (!strcmp (Cmd_Argv(0), y->name))
			return;
	}

	//r1ch: pointless if !u->name, why would we be here?
	if (sv.state == ss_game)
		ge->ClientCommand (sv_player);
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/
static void SV_ClientThink (client_t *cl, usercmd_t *cmd)
{
	cl->commandMsec -= cmd->msec;

	if (cl->commandMsec < 0 && sv_enforcetime->intvalue)
		return;

	ge->ClientThink (cl->edict, cmd);
}

#define	MAX_STRINGCMDS			8
#define	MAX_USERINFO_UPDATES	8
/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
void SV_ExecuteClientMessage (client_t *cl)
{
	int		c;
	char	*s;
	usercmd_t	oldest, oldcmd, newcmd;
	int		net_drop;
	int		stringCmdCount;
	int		userinfoCount;
	//int		checksum, calculatedChecksum;
	//int		checksumIndex;
	qboolean	move_issued;
	int		lastframe;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = false;

	userinfoCount = 0;
	stringCmdCount = 0;

	for (;;)
	{
		if (net_message.readcount > net_message.cursize)
		{
			SV_KickClient (cl, "bad read", "SV_ExecuteClientMessage: bad read\n");
			return;
		}

		c = MSG_ReadByte (&net_message);
		if (c == -1)
			break;

		switch (c)
		{
		case clc_move:
			if (move_issued)
			{
				SV_KickClient (cl, "client issued clc_move when move_issued", "SV_ExecuteClientMessage: clc_move when move_issued\n");
				return;		// someone is trying to cheat...
			}

			move_issued = true;
			//checksumIndex = net_message.readcount;

			//r1ch: suck up the extra checksum byte that is no longer used
			if (cl->protocol == ORIGINAL_PROTOCOL_VERSION)
			{
				MSG_ReadByte (&net_message);
			}
			else if (cl->protocol != ENHANCED_PROTOCOL_VERSION)
			{
				Com_Printf ("SV_ReadClientMessage: bad protocol %d (memory overwritten!)\n", cl->protocol);
				SV_KickClient (cl, "client state corrupted", "SV_ExecuteClientMessage: client state corrupted\n");
				return;
			}

			lastframe = MSG_ReadLong (&net_message);

			//r1ch: allow server admins to stop clients from using nodelta
			//note, this doesn't affect server->client if the clients frame
			//was too old (ie client lagged out) so should be safe to enable
			//nodelta clients typically consume 4-5x bandwidth than normal.
			if (lastframe == -1 && cl->lastframe == -1)
			{
				if (++cl->nodeltaframes >= 100 && !sv_allownodelta->intvalue)
				{
					SV_KickClient (cl, "client is using cl_nodelta", "ERROR: You may not use cl_nodelta on this server as it consumes excessive bandwidth. Please set cl_nodelta 0 if you wish to be able to play on this server.\n");
					return;
				}
			}
			else
			{
				cl->nodeltaframes = 0;
			}

			if (lastframe != cl->lastframe)
			{
				cl->lastframe = lastframe;
				if (cl->lastframe > 0)
				{
					//FIXME: should we adjust for FPS latency?
					cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] = 
						svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;

					//if (cl->fps)
					//	cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] -= 1000 / (cl->fps * 2);
				}
			}

			//memset (&nullcmd, 0, sizeof(nullcmd));

			//r1: check there are actually enough usercmds in the message!
			MSG_ReadDeltaUsercmd (&net_message, &null_usercmd, &oldest);
			if (net_message.readcount > net_message.cursize)
			{
				SV_KickClient (cl, "bad clc_move usercmd read (oldest)", NULL);
				return;
			}

			MSG_ReadDeltaUsercmd (&net_message, &oldest, &oldcmd);
			if (net_message.readcount > net_message.cursize)
			{
				SV_KickClient (cl, "bad clc_move usercmd read (oldcmd)", NULL);
				return;
			}

			MSG_ReadDeltaUsercmd (&net_message, &oldcmd, &newcmd);
			if (net_message.readcount > net_message.cursize)
			{
				SV_KickClient (cl, "bad clc_move usercmd read (newcmd)", NULL);
				return;
			}

			//r1: normal q2 client caps at 250 internally so this is a nice hack check
			if (cl->state == cs_spawned && newcmd.msec > 250)
			{
				Blackhole (&cl->netchan.remote_address, true, "illegal msec value (%d)", newcmd.msec);
				SV_KickClient (cl, "illegal pmove msec detected", NULL);
				return;
			}

			if ( cl->state != cs_spawned )
			{
				cl->lastframe = -1;
				break;
			}

			//r1: reset idle time on activity
			if (newcmd.buttons != oldcmd.buttons ||
				newcmd.forwardmove != oldcmd.forwardmove ||
				newcmd.upmove != oldcmd.upmove)
				cl->idletime = 0;

			// if the checksum fails, ignore the rest of the packet
			//r1ch: removed, this has been hacked to death anyway
			/*calculatedChecksum = COM_BlockSequenceCRCByte (
				net_message.data + checksumIndex + 1,
				net_message.readcount - checksumIndex - 1,
				cl->netchan.incoming_sequence);

			if (calculatedChecksum != checksum)
			{
				Com_DPrintf ("Failed command checksum for %s (%d != %d)/%d\n", 
					cl->name, calculatedChecksum, checksum, 
					cl->netchan.incoming_sequence);
				return;
			}*/

			if (!sv_paused->intvalue)
			{
				net_drop = cl->netchan.dropped;

				//r1: server configurable command backup limit
				if (net_drop < sv_max_netdrop->intvalue)
				{

//if (net_drop > 2)

//	Com_Printf ("drop %i\n", net_drop);
					while (net_drop > 2)
					{
						SV_ClientThink (cl, &cl->lastcmd);

						net_drop--;
					}
					if (net_drop > 1)
						SV_ClientThink (cl, &oldest);

					if (net_drop > 0)
						SV_ClientThink (cl, &oldcmd);

				}
				SV_ClientThink (cl, &newcmd);
			}

			cl->lastcmd = newcmd;
			break;

		case clc_stringcmd:
			c = net_message.readcount;
			s = MSG_ReadString (&net_message);

			//r1: another security check, client caps at 256+1, but a hacked client could
			//    send huge strings, if they are then used in a mod which sends a %s cprintf
			//    to the exe, this could result in a buffer overflow for example.

			// XXX: where is this capped?
			c = net_message.readcount - c;
			if (c > 256)
			{
				//Com_Printf ("%s: excessive stringcmd discarded.\n", cl->name);
				//break;
				Com_Printf ("Warning, %d byte stringcmd from %s: '%.32s...'\n", c, cl->name, s);
			}

			if (move_issued)
				Com_Printf ("Warning, out-of-order stringcmd '%.32s...' from %s\n", s, cl->name);

			// malicious users may try using too many string commands
			if (++stringCmdCount < MAX_STRINGCMDS)
				SV_ExecuteUserCommand (s);

			if (cl->state == cs_zombie)
				return;	// disconnect command
			break;

		case clc_userinfo:
			//r1: limit amount of userinfo per packet
			if (++userinfoCount < MAX_USERINFO_UPDATES)
			{
				strncpy (cl->userinfo, MSG_ReadString (&net_message), sizeof(cl->userinfo)-1);
				SV_UserinfoChanged (cl);
			}
			else
			{
				Com_Printf ("Warning, too many userinfo updates (%d) in single packet from %s\n", userinfoCount, cl->name);
				MSG_ReadString (&net_message);
			}

			if (move_issued)
				Com_Printf ("Warning, out-of-order userinfo from %s\n", cl->name);

			if (cl->state == cs_zombie)
				return;	//kicked

			break;

			//FIXME: remove this?
		case clc_nop:
			break;

		default:
			Com_Printf ("SV_ExecuteClientMessage: unknown command byte %d\n", c);
			SV_KickClient (cl, "unknown command byte", va("unknown command byte %d\n", c));
			return;
		}
	}
}
