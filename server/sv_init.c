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

#include "server.h"

#ifdef WIN32
#include <process.h>
#endif

server_static_t	svs;				// persistant server info
server_t		sv;					// local server

int sv_download_socket = 0;

/*
================
SV_FindIndex

================
*/
int SV_FindIndex (char *name, int start, int maxIndex, qboolean create)
{
	int		i;
	
	if (!name || !name[0])
	{
		Com_DPrintf ("SV_FindIndex: NULL or empty name, ignored\n");
		if (sv_gamedebug->value >= 3)
			DEBUGBREAKPOINT;
		return 0;
	}

	for (i=1 ; i<maxIndex && sv.configstrings[start+i][0] ; i++)
	{
		if (!strcmp(sv.configstrings[start+i], name)) {
			return i;
		}
	}

	if (!create)
		return 0;

	if (i == maxIndex) {
		Com_Printf ("ERROR: Ran out of configstrings while attempting to add '%s' (%d,%d)\n", name, start, maxIndex);
		Com_Printf ("Dumping configstrings in use to 'configstrings.txt'...");
		{
			FILE *cs;
			cs = fopen ("configstrings.txt", "wb");
			if (!cs) {
				Com_Printf ("failed.\n");
			} else {
				fprintf (cs, "r1q2 configstring dump:\n\nCS_SOUNDS:\n");
				for (i = CS_SOUNDS; i < CS_SOUNDS+MAX_SOUNDS; i++)
					fprintf (cs, "%i: %s\n", i, sv.configstrings[i]);

				fprintf (cs, "\nCS_MODELS:\n");
				for (i = CS_MODELS; i < CS_MODELS+MAX_MODELS; i++)
					fprintf (cs, "%i: %s\n", i, sv.configstrings[i]);

				fprintf (cs, "\nCS_ITEMS:\n");
				for (i = CS_MODELS; i < CS_MODELS+MAX_MODELS; i++)
					fprintf (cs, "%i: %s\n", i, sv.configstrings[i]);

				fprintf (cs, "\nCS_IMAGES:\n");
				for (i = CS_IMAGES; i < CS_IMAGES+MAX_IMAGES; i++)
					fprintf (cs, "%i: %s\n", i, sv.configstrings[i]);

				fprintf (cs, "\nCS_LIGHTS:\n");
				for (i = CS_LIGHTS; i < CS_IMAGES+MAX_LIGHTSTYLES; i++)
					fprintf (cs, "%i: %s\n", i, sv.configstrings[i]);

				fprintf (cs, "\nCS_GENERAL:\n");
				for (i = CS_GENERAL; i < CS_GENERAL+MAX_GENERAL; i++)
					fprintf (cs, "%i: %s\n", i, sv.configstrings[i]);

				fclose (cs);
				Com_Printf ("done.\n");
			}
		}
		Com_Error (ERR_GAME, "SV_FindIndex: overflow finding index for %s", name);
	}

	strncpy (sv.configstrings[start+i], name, sizeof(sv.configstrings[i]));

	if (sv.state != ss_loading)
	{	// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_BeginWriteByte (&sv.multicast, svc_configstring);
		MSG_WriteShort (&sv.multicast, start+i);
		MSG_WriteString (&sv.multicast, name);
		SV_Multicast (vec3_origin, MULTICAST_ALL_R);
		return i;
	}

	return i;
}


int EXPORT SV_ModelIndex (char *name)
{
	return SV_FindIndex (name, CS_MODELS, MAX_MODELS, true);
}

int EXPORT SV_SoundIndex (char *name)
{
	return SV_FindIndex (name, CS_SOUNDS, MAX_SOUNDS, true);
}

int EXPORT SV_ImageIndex (char *name)
{
	return SV_FindIndex (name, CS_IMAGES, MAX_IMAGES, true);
}

/*
=================
SV_CheckForSavegame
=================
*/
void SV_CheckForSavegame (void)
{
	char		name[MAX_OSPATH];
	FILE		*f;
	int			i;

	if (sv_noreload->value)
		return;

	if (Cvar_VariableValue ("deathmatch"))
		return;

	Com_sprintf (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	f = fopen (name, "rb");
	if (!f)
		return;		// no savegame

	fclose (f);

	SV_ClearWorld ();

	// get configstrings and areaportals
	SV_ReadLevelFile ();

	if (!sv.loadgame)
	{	// coming back to a level after being in a different
		// level, so run it for ten seconds

		// rlava2 was sending too many lightstyles, and overflowing the
		// reliable data. temporarily changing the server state to loading
		// prevents these from being passed down.
		server_state_t		previousState;		// PGM

		previousState = sv.state;				// PGM
		sv.state = ss_loading;					// PGM
		for (i=0 ; i<100 ; i++)
			ge->RunFrame ();

		sv.state = previousState;				// PGM
	}
}


/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer (char *server, char *spawnpoint, server_state_t serverstate, qboolean attractloop, qboolean loadgame)
{
	int			i;
	unsigned	checksum;

	if (attractloop)
		Cvar_Set ("paused", "0");

	if (sv_recycle->value)
	{
		Com_Printf ("SV_SpawnServer: Reloading Game DLL.\n");
		SV_InitGameProgs();
		Cvar_ForceSet ("sv_recycle", "0");
	}

	Com_Printf ("------- Server Initialization -------\n");

	Com_DPrintf ("SpawnServer: %s\n",server);
	if (sv.demofile)
		fclose (sv.demofile);

	svs.spawncount++;		// any partially connected client will be
							// restarted
	sv.state = ss_dead;
	Com_SetServerState (sv.state);

	// wipe the entire per-level structure
	memset (&sv, 0, sizeof(sv));

	//FIXME: this breaks a bunch of things
	if (sv_randomframe->value)
		sv.framenum = random() * 0x00FFFFFF;

	svs.realtime = 0;
	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	// save name for levels that don't set message
	strcpy (sv.configstrings[CS_NAME], server);
	if (Cvar_VariableValue ("deathmatch"))
	{
		Com_sprintf(sv.configstrings[CS_AIRACCEL], sizeof(sv.configstrings[CS_AIRACCEL]), "%g", sv_airaccelerate->value);
		pm_airaccelerate = sv_airaccelerate->value;
	}
	else
	{
		strcpy(sv.configstrings[CS_AIRACCEL], "0");
		pm_airaccelerate = 0;
	}

	SZ_Init (&sv.multicast, sv.multicast_buf, sizeof(sv.multicast_buf));

	strcpy (sv.name, server);

	// leave slots at start for clients only
	for (i=0 ; i<maxclients->value ; i++)
	{
		// needs to reconnect
		if (svs.clients[i].state > cs_connected)
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	sv.time = 1000;
	
	//strcpy (sv.name, server);
	//strcpy (sv.configstrings[CS_NAME], server);

	if (serverstate != ss_game)
	{
		sv.models[1] = CM_LoadMap ("", false, &checksum);	// no real map
	}
	else
	{
		Com_sprintf (sv.configstrings[CS_MODELS+1],sizeof(sv.configstrings[CS_MODELS+1]),
			"maps/%s.bsp", server);
		sv.models[1] = CM_LoadMap (sv.configstrings[CS_MODELS+1], false, &checksum);
	}

	Com_sprintf (sv.configstrings[CS_MAPCHECKSUM],sizeof(sv.configstrings[CS_MAPCHECKSUM]),
		"%i", checksum);

	//
	// clear physics interaction links
	//
	SV_ClearWorld ();
	
	for (i=1 ; i< CM_NumInlineModels() ; i++)
	{
		Com_sprintf (sv.configstrings[CS_MODELS+1+i], sizeof(sv.configstrings[CS_MODELS+1+i]),
			"*%i", i);
		sv.models[i+1] = CM_InlineModel (sv.configstrings[CS_MODELS+1+i]);
	}

	//
	// spawn the rest of the entities on the map
	//	

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Com_SetServerState (sv.state);

	// load and spawn all other entities
	ge->SpawnEntities ( sv.name, CM_EntityString(), spawnpoint );

	// run two frames to allow everything to settle
	ge->RunFrame ();
	ge->RunFrame ();

	// all precaches are complete
	sv.state = serverstate;
	Com_SetServerState (sv.state);
	
	// create a baseline for more efficient communications

	//r1: baslines are now allocated on a per client basis
	//SV_CreateBaseline ();

	// check for a savegame
	SV_CheckForSavegame ();

	// set serverinfo variable
	Cvar_FullSet ("mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Com_Printf ("-------------------------------------\n");
}

/*
==============
SV_InitGame

A brand new game has been started
==============
*/
void SV_InitGame (void)
{
	int		i;
	edict_t	*ent;
	char	idmaster[32];

	if (svs.initialized)
	{
		// cause any connected clients to reconnect
		SV_Shutdown ("Server restarted\n", true, false);
	}
#ifndef DEDICATED_ONLY
	else
	{
		// make sure the client is down
		CL_Drop (false);
		SCR_BeginLoadingPlaque ();
	}
#endif
	// get any latched variable changes (maxclients, etc)
	Cvar_GetLatchedVars ();

	svs.initialized = true;

	if (Cvar_VariableValue ("coop") && Cvar_VariableValue ("deathmatch"))
	{
		Com_Printf("Deathmatch and Coop both set, disabling Coop\n");
		Cvar_FullSet ("coop", "0",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// dedicated servers are can't be single player and are usually DM
	// so unless they explicity set coop, force it to deathmatch
	if (dedicated->value)
	{
		if (!Cvar_VariableValue ("coop"))
			Cvar_FullSet ("deathmatch", "1",  CVAR_SERVERINFO | CVAR_LATCH);
	}

	// init clients
	if (Cvar_VariableValue ("deathmatch"))
	{
		if (maxclients->value <= 1)
			Cvar_FullSet ("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
		else if (maxclients->value > MAX_CLIENTS)
			Cvar_FullSet ("maxclients", va("%i", MAX_CLIENTS), CVAR_SERVERINFO | CVAR_LATCH);
	}
	else if (Cvar_VariableValue ("coop"))
	{
		if (maxclients->value <= 1 || maxclients->value > MAX_CLIENTS)
			Cvar_FullSet ("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
	}
	else	// non-deathmatch, non-coop is one player
	{
		Cvar_FullSet ("maxclients", "1", CVAR_SERVERINFO | CVAR_LATCH);
	}

	svs.spawncount = randomMT()&0x7FFFFFFF;
	svs.clients = Z_TagMalloc (sizeof(client_t)*maxclients->value, TAGMALLOC_CLIENTS);
	svs.num_client_entities = maxclients->value*UPDATE_BACKUP*64;
	svs.client_entities = Z_TagMalloc (sizeof(entity_state_t)*svs.num_client_entities, TAGMALLOC_CL_ENTS);

	// r1: spam warning for those stupid servers that run 250 maxclients and 32 player slots
	if (maxclients->value > 64)
		Com_Printf ("WARNING: Setting maxclients higher than the maximum number of players you intend to have playing can negatively affect server performance and bandwidth use.\n");

	// init network stuff
	if (maxclients->value > 1)
		NET_Config (NET_SERVER);

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = -99999;		// send immediately
	Com_sprintf(idmaster, sizeof(idmaster), "192.246.40.37:%i", PORT_MASTER);
	NET_StringToAdr (idmaster, &master_adr[0]);

	//r1: tcp download port (off in release)
#ifdef _DEBUG
	sv_downloadport = Cvar_Get ("sv_downloadport", va("%d", server_port), 0);
#else
	sv_downloadport = Cvar_Get ("sv_downloadport", "0", 0);
#endif

#ifdef _DEBUG
	if (sv_downloadport->value)
	{
		sv_download_socket = NET_Listen ((unsigned short)sv_downloadport->value);
		if (sv_download_socket == -1)
		{
			Com_Printf ("SV_InitGame: couldn't listen on %d!\n", (int)sv_downloadport->value);
			sv_download_socket = 0;
		}

		Com_Printf ("DownloadServer running on port %d, socket %d\n", (int)sv_downloadport->value, sv_download_socket);
	}
#endif

	// init game
	SV_InitGameProgs ();
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = EDICT_NUM(i+1);
		ent->s.number = i+1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
}


/*
======================
SV_Map

  the full syntax is:

  map [*]<map>$<startspot>+<nextserver>

command from the console or progs.
Map can also be a.cin, .pcx, or .dm2 file
Nextserver is used to allow a cinematic to play, then proceed to
another level:

	map tram.cin+jail_e3
======================
*/
void SV_Map (qboolean attractloop, char *levelstring, qboolean loadgame)
{
	char	level[MAX_QPATH];
	char	*ch;
	int		l;
	char	spawnpoint[MAX_QPATH];

	sv.loadgame = loadgame;
	sv.attractloop = attractloop;

	//FIXME: EVIL FIX THIS
	if (sv.state == ss_dead && !sv.loadgame) {
		char old_game[MAX_QPATH];
		//r1: attract loop = demo = no game dll should be used except baseq2 as it might interfere
		if (attractloop) {
			strcpy (old_game,  Cvar_VariableString ("game"));
			Cvar_Set ("game", BASEDIRNAME);
		}
		SV_InitGame ();	// the game is just starting
		if (attractloop)
			Cvar_Set ("game", old_game);
	}

	//r1: yet another buffer overflow was here.
	strncpy (level, levelstring, sizeof(level)-1);

	// if there is a + in the map, set nextserver to the remainder
	ch = strstr(level, "+");
	if (ch)
	{
		*ch = 0;
		Cvar_Set ("nextserver", va("gamemap \"%s\"", ch+1));
	}
	else
		Cvar_Set ("nextserver", "");

	//ZOID special hack for end game screen in coop mode
	if (Cvar_VariableValue ("coop") && !Q_stricmp(level, "victory.pcx"))
		Cvar_Set ("nextserver", "gamemap \"*base1\"");

	// if there is a $, use the remainder as a spawnpoint
	ch = strstr(level, "$");
	if (ch)
	{
		*ch = 0;
		strcpy (spawnpoint, ch+1);
	}
	else
		spawnpoint[0] = 0;

	// skip the end-of-unit flag if necessary
	//r1: should be using memmove for this, overlapping strcpy = unreliable
	if (level[0] == '*')
		//strcpy (level, level+1);
		memmove (level, level+1, strlen(level)+1);

	l = strlen(level);
	if (l > 4 && !strcmp (level+l-4, ".cin") )
	{
#ifndef DEDICATED_ONLY
		SCR_BeginLoadingPlaque ();			// for local system
#endif
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, ss_cinematic, attractloop, loadgame);
	}
	else if (l > 4 && !strcmp (level+l-4, ".dm2") )
	{
#ifndef DEDICATED_ONLY
		SCR_BeginLoadingPlaque ();			// for local system
#endif
		SV_BroadcastCommand ("changing\n");
		sv.attractloop = attractloop = 1;
		SV_SpawnServer (level, spawnpoint, ss_demo, attractloop, loadgame);
	}
	else if (l > 4 && !strcmp (level+l-4, ".pcx") )
	{
#ifndef DEDICATED_ONLY
		SCR_BeginLoadingPlaque ();			// for local system
#endif
		SV_BroadcastCommand ("changing\n");
		SV_SpawnServer (level, spawnpoint, ss_pic, attractloop, loadgame);
	}
	else
	{
#ifndef DEDICATED_ONLY
		SCR_BeginLoadingPlaque ();			// for local system
#endif
		SV_BroadcastCommand ("changing\n");
		SV_SendClientMessages ();
		SV_SpawnServer (level, spawnpoint, ss_game, attractloop, loadgame);

		//r1: do we really need this?
		//Cbuf_CopyToDefer ();
	}

	SV_BroadcastCommand ("reconnect\n");
}
