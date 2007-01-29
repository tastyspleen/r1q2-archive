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

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
SV_SetMaster_f

Specify a list of master servers
====================
*/
static void SV_SetMaster_f (void)
{
	int		i, slot;

	// only dedicated servers send heartbeats
	if (!dedicated->intvalue)
	{
		Com_Printf ("Only dedicated servers use masters.\n", LOG_GENERAL);
		return;
	}

	// make sure the server is listed public
	Cvar_Set ("public", "1");

	memset (&master_adr, 0, sizeof(master_adr));

	slot = 0;		// slot 0 will not always contain the id master
	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		if (slot == MAX_MASTERS)
			break;

		if (!NET_StringToAdr (Cmd_Argv(i), &master_adr[slot]))
		{
			Com_Printf ("Bad address: %s\n", LOG_GENERAL, Cmd_Argv(i));
			continue;
		}

		if (master_adr[slot].port == 0)
			master_adr[slot].port = ShortSwap (PORT_MASTER);

		Com_Printf ("Master server at %s\n", LOG_GENERAL, NET_AdrToString (&master_adr[slot]));

		if (svs.initialized)
		{
			Com_Printf ("Sending a ping.\n", LOG_GENERAL|LOG_NOTICE);

			Netchan_OutOfBandPrint (NS_SERVER, &master_adr[slot], "ping");
		}

		slot++;
	}

	svs.last_heartbeat = -9999999;
}

qboolean StringIsNumeric (const char *s)
{
	const char	*p;

	if (!s[0])
		return false;

	p = s;

	while (p[0])
	{
		if (!isdigit (p[0]))
			return false;
		p++;
	}

	return true;
}

static int MaskBits (uint32 mask)
{
	int		i;
	int		bits;

	bits = 0;

	for (i = 0; i < 32; i++)
	{
		if (mask & (1 << i))
			bits++;
	}

	return bits;
}

static qboolean ValidateIPMask (char *ip, netadr_t *from, int *outmask)
{
	char	*s;
	int		mask;

	s = strchr (ip, '/');
	if (s)
	{
		s[0] = 0;
		s++;
		if (!*s)
		{
			Com_Printf ("Invalid IP mask.\n", LOG_GENERAL);
			return false;
		}
		mask = atoi (s);
		if (mask < 0 || mask > 32)
		{
			Com_Printf ("Invalid IP mask.\n", LOG_GENERAL);
			return false;
		}
	}
	else
	{
		mask = 32;
	}

	if (!(NET_StringToAdr (ip, from)))
	{
		Com_Printf ("Can't find address for '%s'\n", LOG_GENERAL, ip);
		return false;
	}

	*outmask = mask;

	return true;
}

static void DumpNetBlockList (netblock_t *list)
{
	while (list->next)
	{
		list = list->next;

		Com_Printf ("%s/%d\n", LOG_GENERAL, NET_inet_ntoa (NET_ntohl(list->ip)), MaskBits (NET_ntohl (list->mask)));
	}
}

/*
==================
SV_SetPlayer

Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
static qboolean SV_SetPlayer (void)
{
	client_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	if (Cmd_Argc() < 2)
		return false;

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if (StringIsNumeric (s))
	{
		idnum = atoi(s);
		if (idnum < 0 || idnum >= maxclients->intvalue)
		{
			Com_Printf ("Bad client slot: %i\n", LOG_GENERAL, idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (sv_client->state <= cs_zombie)
		{
			Com_Printf ("Client %i is not active\n", LOG_GENERAL, idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i=0,cl=svs.clients ; i<maxclients->intvalue; i++,cl++)
	{
		if (cl->state <= cs_zombie)
			continue;

		if (!strcmp(cl->name, s))
		{
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}

	// check for a subname match
	if (sv_enhanced_setplayer->intvalue)
	{
		for (i=0,cl=svs.clients ; i<maxclients->intvalue; i++,cl++)
		{
			if (cl->state <= cs_zombie)
				continue;

			if (strstr(cl->name, s))
			{
				sv_client = cl;
				sv_player = sv_client->edict;
				return true;
			}
		}
	}

	Com_Printf ("Userid %s is not on the server\n", LOG_GENERAL, s);
	return false;
}


/*
===============================================================================

SAVEGAME FILES

===============================================================================
*/

/*
=====================
SV_WipeSavegame

Delete save/<XXX>/
=====================
*/
static void SV_WipeSavegame (char *savename)
{
	char	name[MAX_OSPATH];
	char	*s;

	Com_DPrintf("SV_WipeSaveGame(%s)\n", savename);

	Com_sprintf (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir (), savename);
	remove (name);
	Com_sprintf (name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir (), savename);
	remove (name);

	Com_sprintf (name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir (), savename);
	s = Sys_FindFirst( name, 0, 0 );
	while (s)
	{
		remove (s);
		s = Sys_FindNext( 0, 0 );
	}
	Sys_FindClose ();
	Com_sprintf (name, sizeof(name), "%s/save/%s/*.sv2", FS_Gamedir (), savename);
	s = Sys_FindFirst(name, 0, 0 );
	while (s)
	{
		remove (s);
		s = Sys_FindNext( 0, 0 );
	}
	Sys_FindClose ();
}


/*
================
qCopyFile
================
*/
static void qCopyFile (char *src, char *dst)
{
	FILE	*f1, *f2;
	size_t	l;
	byte	buffer[65536];

	Com_DPrintf ("qCopyFile (%s, %s)\n", src, dst);

	f1 = fopen (src, "rb");
	if (!f1)
		return;
	f2 = fopen (dst, "wb");
	if (!f2)
	{
		fclose (f1);
		return;
	}

	for (;;)
	{
		l = fread (buffer, 1, sizeof(buffer), f1);
		if (!l)
		{
			if (ferror (f1))
				Com_Printf ("WARNING: qCopyFile: fread() failed.\n", LOG_GENERAL|LOG_WARNING);
			break;
		}

		fwrite (buffer, 1, l, f2);
	}

	fclose (f1);
	fclose (f2);
}


/*
================
SV_CopySaveGame
================
*/
static void SV_CopySaveGame (char *src, char *dst)
{
	char	name[MAX_OSPATH], name2[MAX_OSPATH];
	size_t	l;
	size_t	len;
	char	*found;

	Com_DPrintf("SV_CopySaveGame(%s, %s)\n", src, dst);

	SV_WipeSavegame (dst);

	// copy the savegame over
	Com_sprintf (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), src);
	Com_sprintf (name2, sizeof(name2), "%s/save/%s/server.ssv", FS_Gamedir(), dst);
	FS_CreatePath (name2);
	qCopyFile (name, name2);

	Com_sprintf (name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir(), src);
	Com_sprintf (name2, sizeof(name2), "%s/save/%s/game.ssv", FS_Gamedir(), dst);
	qCopyFile (name, name2);

	Com_sprintf (name, sizeof(name), "%s/save/%s/", FS_Gamedir(), src);
	len = strlen(name);
	Com_sprintf (name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), src);
	found = Sys_FindFirst(name, 0, 0 );

	while (found)
	{
		strcpy (name+len, found+len);

		Com_sprintf (name2, sizeof(name2), "%s/save/%s/%s", FS_Gamedir(), dst, found+len);
		qCopyFile (name, name2);

		// change sav to sv2
		l = strlen(name);
		strcpy (name+l-3, "sv2");
		l = strlen(name2);
		strcpy (name2+l-3, "sv2");
		qCopyFile (name, name2);

		found = Sys_FindNext( 0, 0 );
	}
	Sys_FindClose ();
}


/*
==============
SV_WriteLevelFile

==============
*/
static void SV_WriteLevelFile (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;

	Com_DPrintf("SV_WriteLevelFile()\n");

	Com_sprintf (name, sizeof(name), "%s/save/current/%s.sv2", FS_Gamedir(), sv.name);
	f = fopen(name, "wb");
	if (!f)
	{
		Com_Printf ("Failed to open %s\n", LOG_GENERAL, name);
		return;
	}
	fwrite (sv.configstrings, sizeof(sv.configstrings), 1, f);
	CM_WritePortalState (f);
	fclose (f);

	Com_sprintf (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	ge->WriteLevel (name);
}

/*
==============
SV_ReadLevelFile

==============
*/
void SV_ReadLevelFile (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;

	Com_DPrintf("SV_ReadLevelFile()\n");

	Com_sprintf (name, sizeof(name), "%s/save/current/%s.sv2", FS_Gamedir(), sv.name);
	f = fopen(name, "rb");
	if (!f)
	{
		Com_Printf ("Failed to open %s\n", LOG_GENERAL, name);
		return;
	}
	FS_Read (sv.configstrings, sizeof(sv.configstrings), f);
	CM_ReadPortalState (f);
	fclose (f);

	Com_sprintf (name, sizeof(name), "%s/save/current/%s.sav", FS_Gamedir(), sv.name);
	ge->ReadLevel (name);
}

/*
==============
SV_WriteServerFile

==============
*/
static void SV_WriteServerFile (qboolean autosave)
{
	FILE	*f;
	cvar_t	*var;
	char	name[MAX_OSPATH], string[128];
	char	comment[32];
	time_t	aclock;
	struct tm	*newtime;

	Com_DPrintf("SV_WriteServerFile(%s)\n", autosave ? "true" : "false");

	Com_sprintf (name, sizeof(name), "%s/save/current/server.ssv", FS_Gamedir());
	f = fopen (name, "wb");
	if (!f)
	{
		Com_Printf ("Couldn't write %s\n", LOG_GENERAL, name);
		return;
	}
	// write the comment field
	memset (comment, 0, sizeof(comment));

	if (!autosave)
	{
		time (&aclock);
		newtime = localtime (&aclock);
		Com_sprintf (comment,sizeof(comment), "%2i:%i%i %2i/%2i  ", newtime->tm_hour
			, newtime->tm_min/10, newtime->tm_min%10,
			newtime->tm_mon+1, newtime->tm_mday);
		strncat (comment, sv.configstrings[CS_NAME], sizeof(comment)-1-strlen(comment) );
	}
	else

	{	// autosaved
		Com_sprintf (comment, sizeof(comment), "ENTERING %s", sv.configstrings[CS_NAME]);
	}

	fwrite (comment, 1, sizeof(comment), f);

	// write the mapcmd
	fwrite (svs.mapcmd, 1, sizeof(svs.mapcmd), f);

	// write all CVAR_LATCH cvars
	// these will be things like coop, skill, deathmatch, etc
	for (var = cvar_vars ; var ; var=var->next)
	{
		if (!(var->flags & CVAR_LATCH))
			continue;
		if (strlen(var->name) >= sizeof(name)-1
			|| strlen(var->string) >= sizeof(string)-1)
		{
			Com_Printf ("Cvar too long: %s = %s\n", LOG_SERVER|LOG_WARNING, var->name, var->string);
			continue;
		}
		memset (name, 0, sizeof(name));
		memset (string, 0, sizeof(string));
		strcpy (name, var->name);
		strcpy (string, var->string);
		fwrite (name, 1, sizeof(name), f);
		fwrite (string, 1, sizeof(string), f);
	}

	fclose (f);

	// write game state
	Com_sprintf (name, sizeof(name), "%s/save/current/game.ssv", FS_Gamedir());
	ge->WriteGame (name, autosave);
}

/*
==============
SV_ReadServerFile

==============
*/
static void SV_ReadServerFile (void)
{
	FILE	*f;
	char	name[MAX_OSPATH], string[128];
	char	comment[32];
	char	mapcmd[MAX_TOKEN_CHARS];

	Com_DPrintf("SV_ReadServerFile()\n");

	Com_sprintf (name, sizeof(name), "%s/save/current/server.ssv", FS_Gamedir());
	f = fopen (name, "rb");
	if (!f)
	{
		Com_Printf ("Couldn't read %s\n", LOG_GENERAL, name);
		return;
	}
	// read the comment field
	FS_Read (comment, sizeof(comment), f);

	// read the mapcmd
	FS_Read (mapcmd, sizeof(mapcmd), f);

	// read all CVAR_LATCH cvars
	// these will be things like coop, skill, deathmatch, etc
	//while (1)
	for (;;)
	{
		if (!fread (name, 1, sizeof(name), f))
			break;
		FS_Read (string, sizeof(string), f);
		Com_DPrintf ("Set %s = %s\n", name, string);
		Cvar_ForceSet (name, string);
	}

	fclose (f);

	// start a new game fresh with new cvars
	SV_InitGame ();

	strcpy (svs.mapcmd, mapcmd);

	// read game state
	Com_sprintf (name, sizeof(name), "%s/save/current/game.ssv", FS_Gamedir());
	ge->ReadGame (name);
}


//=========================================================




/*
==================
SV_DemoMap_f

Puts the server in demo mode on a specific map/cinematic
==================
*/
static void SV_DemoMap_f (void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Purpose: View a recorded demo without loading a Game DLL.\n"
					"Syntax : demomap <filename>\n"
					"Example: demomap demo1.dm2\n", LOG_GENERAL);
		//Com_Printf ("USAGE: demomap <demoname.dm2>\n");
		return;
	}

	SV_Map (true, Cmd_Argv(1), false );
}

/*
==================
SV_GameMap_f

Saves the state of the map just being exited and goes to a new map.

If the initial character of the map string is '*', the next map is
in a new unit, so the current savegame directory is cleared of
map files.

Example:

*inter.cin+jail

Clears the archived maps, plays the inter.cin cinematic, then
goes to map jail.bsp.
==================
*/
static void SV_GameMap_f (void)
{
	char		expanded[MAX_QPATH];
	char		map[MAX_TOKEN_CHARS];
	int			i;
	client_t	*cl;

	if (Cmd_Argc() != 2)
	{
		//Com_Printf ("USAGE: gamemap <mapname>\n");
		Com_Printf ("Purpose: Change the level.\n"
					"Syntax : gamemap <mapname>\n"
					"Example: gamemap q2dm1\n", LOG_GENERAL);
		return;
	}

	Com_DPrintf("SV_GameMap(%s)\n", Cmd_Argv(1));

	//SV_Map nukes cmd_arg*
	Q_strncpy (map, Cmd_Argv(1), sizeof(map)-1);

	FS_CreatePath (va("%s/save/current/", FS_Gamedir()));

	// check for clearing the current savegame
	if (map[0] == '*')
	{
		// wipe all the *.sav files
		SV_WipeSavegame ("current");
	}
	else
	{	// save the map just exited
		if (!strchr (map, '.') && !strchr (map, '$'))
		{
			Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);

			//r1: check it exists
            //if (FS_LoadFile (expanded, NULL) == -1)
			if (!CM_MapWillLoad (expanded))
			{
				Com_Printf ("Can't find map '%s'\n", LOG_GENERAL, map);
				return;
			}
		}		
		
		if (sv.state == ss_game)
		{
			qboolean	savedInuse[MAX_CLIENTS];
			// clear all the client inuse flags before saving so that
			// when the level is re-entered, the clients will spawn
			// at spawn points instead of occupying body shells
			//savedInuse = malloc(maxclients->value * sizeof(qboolean));
			for (i=0,cl=svs.clients ; i<maxclients->intvalue; i++,cl++)
			{
				savedInuse[i] = cl->edict->inuse;
				cl->edict->inuse = false;
			}

			SV_WriteLevelFile ();

			// we must restore these for clients to transfer over correctly
			for (i=0,cl=svs.clients ; i<maxclients->intvalue; i++,cl++)
				cl->edict->inuse = savedInuse[i];
			//free (savedInuse);
		}
	}

	// start up the next map
	SV_Map (false, map, false );

	// archive server state
	strncpy (svs.mapcmd, map, sizeof(svs.mapcmd)-1);

	// copy off the level to the autosave slot
	if (!dedicated->intvalue && !Cvar_IntValue ("deathmatch"))
	{
		SV_WriteServerFile (true);
		SV_CopySaveGame ("current", "save0");
	}
}

/*
==================
SV_Map_f

Goes directly to a given map without any savegame archiving.
For development work
==================
*/
static void SV_Map_f (void)
{
	char	*map;
	char	expanded[MAX_QPATH];
	//extern cvar_t	*fs_gamedirvar;

	if (Cmd_Argc() != 2)
	{
		//Com_Printf ("USAGE: map <mapname>\n");
		Com_Printf ("Purpose: Reset game state and begin a level.\n"
					"Syntax : map <mapname>\n"
					"Example: map q2dm1\n", LOG_GENERAL);
		return;
	}

	if (sv.state == ss_game && Cvar_GetNumLatchedVars() == 0)
	{
		if (sv_allow_map->intvalue == 0)
		{
			static	qboolean warned = false;

			Com_Printf ("WARNING: Using 'map' will reset the game state. Use 'gamemap' to change levels.\n", LOG_GENERAL);
			if (!warned)
			{
				Com_Printf ("(Set the cvar 'sv_allow_map 1' if you wish to disable this check)\n", LOG_GENERAL);
				warned = true;
			}
			return;
		}
		else if (sv_allow_map->intvalue == 2)
		{
			SV_GameMap_f ();
			return;
		}
	}

	// if not a pcx, demo, or cinematic, check to make sure the level exists
	map = Cmd_Argv(1);
	if (!strchr (map, '.') && !strchr (map, '$') && *map != '*')
	{
		Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);

		//r1: check it exists
        //if (FS_LoadFile (expanded, NULL) == -1)
		if (!CM_MapWillLoad (expanded))
		{			
			Com_Printf ("Can't find map '%s'\n", LOG_GENERAL, map);
			return;
		}
	}

	sv.state = ss_dead;		// don't save current level when changing
	SV_WipeSavegame("current");
	SV_GameMap_f ();
}

/*
=====================================================================

  SAVEGAMES

=====================================================================
*/


/*
==============
SV_Loadgame_f

==============
*/
static void SV_Loadgame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	char	*dir;

	if (Cmd_Argc() != 2)
	{
		//Com_Printf ("USAGE: loadgame <directory>\n");
		Com_Printf ("Purpose: Load a saved game.\n"
					"Syntax : loadgame <directory>\n"
					"Example: loadgame base1\n", LOG_GENERAL);
		return;
	}

	dir = Cmd_Argv(1);
	if (strstr (dir, "..") || strchr (dir, '/') || strchr (dir, '\\') )
	{
		Com_Printf ("Bad savedir.\n", LOG_GENERAL);
		return;
	}

	// make sure the server.ssv file exists
	Com_sprintf (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), Cmd_Argv(1));
	f = fopen (name, "rb");
	if (!f)
	{
		Com_Printf ("No such savegame: %s\n", LOG_GENERAL, name);
		return;
	}
	fclose (f);

	Com_Printf ("Loading game...\n", LOG_GENERAL);

	SV_CopySaveGame (Cmd_Argv(1), "current");

	SV_ReadServerFile ();

	// go to the map
	sv.state = ss_dead;		// don't save current level when changing
	SV_Map (false, svs.mapcmd, true);
}



/*
==============
SV_Savegame_f

==============
*/
static void SV_Savegame_f (void)
{
	char	*dir;

	if (sv.state != ss_game)
	{
		Com_Printf ("You must be in a game to save.\n", LOG_GENERAL);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("USAGE: save <directory>\n", LOG_GENERAL);
		return;
	}

	if (Cvar_IntValue("deathmatch"))
	{
		Com_Printf ("Can't savegame in a deathmatch\n", LOG_GENERAL);
		return;
	}

	if (!strcmp (Cmd_Argv(1), "current"))
	{
		Com_Printf ("Can't save to 'current'\n", LOG_GENERAL);
		return;
	}

	dir = Cmd_Argv(1);
	if (strstr (dir, "..") || strchr (dir, '/') || strchr (dir, '\\') )
	{
		Com_Printf ("Bad savedir.\n", LOG_GENERAL);
		return;
	}

	Com_Printf ("Saving game...\n", LOG_GENERAL);

	// archive current level, including all client edicts.
	// when the level is reloaded, they will be shells awaiting
	// a connecting client
	SV_WriteLevelFile ();

	// save server state
	SV_WriteServerFile (false);

	// copy it off
	SV_CopySaveGame ("current", dir);

	Com_Printf ("Done.\n", LOG_GENERAL);
}

/*static qboolean RemoveCvarBan (int index)
{
	cvarban_t *temp, *last;
	int i;

	i = index;

	last = temp = &cvarbans;
	while (temp->next)
	{
		last = temp;
		temp = temp->next;
		i--;
		if (i == 0)
			break;
		else if (i < 0)
			return false;
	}

	// if list ended before we could get to index
	if (i > 0)
		return false;

	// just copy the next over, don't care if it's null
	last->next = temp->next;

	Z_Free (temp->cvarname);
	Z_Free (temp->matchvalue);
	Z_Free (temp->message);
	Z_Free (temp);

	return true;
}*/

static void SV_CheckCvarBans_f (void)
{
	varban_t *bans = &cvarbans;

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	if (!bans->next)
	{
		Com_Printf ("No cvarbans defined.\n", LOG_GENERAL);
		return;
	}

	while (bans->next)
	{
		bans = bans->next;

		//version is not implicit here
		MSG_BeginWriting (svc_stufftext);
		MSG_WriteString (va("cmd \177c %s $%s\n", bans->varname, bans->varname));
	}

	SV_Multicast (NULL, MULTICAST_ALL_R);
	Com_Printf ("cvar checks sent...\n", LOG_GENERAL);
}

static void ShowVarBans (varban_t *bans)
{
	banmatch_t	*match;
	const char	*mtype;

	while (bans->next)
	{
		bans = bans->next;
		match = &bans->match;
		while (match->next)
		{
			match = match->next;

			switch (match->blockmethod)
			{
				case CVARBAN_LOGONLY:
					mtype = "LOG";
					break;
				case CVARBAN_BLACKHOLE:
					mtype = "BLACKHOLE";
					break;
				case CVARBAN_KICK:
					mtype = "KICK";
					break;
				case CVARBAN_MESSAGE:
					mtype = "MESSAGE";
					break;
				case CVARBAN_STUFF:
					mtype = "STUFF";
					break;
				default:
					mtype = "???";
					break;
			}

			Com_Printf ("%s: %s (%s:%s)\n", LOG_GENERAL, bans->varname, match->matchvalue, mtype, match->message);
		}
	}
}

static void SV_ListCvarBans_f (void)
{
	Com_Printf ("Current cvar bans:\n", LOG_GENERAL);
	ShowVarBans (&cvarbans);
}

static void SV_ListUserinfoBans_f (void)
{
	Com_Printf ("Current userinfo bans:\n", LOG_GENERAL);
	ShowVarBans (&userinfobans);
}

static qboolean DeleteVarBan (varban_t *bans, char *match)
{
	varban_t	*last;

	last = bans;

	while (bans->next)
	{
		bans = bans->next;
		if (!Q_stricmp (match, bans->varname))
		{
			while (bans->match.next)
			{
				banmatch_t *old;
				old = bans->match.next;
				bans->match.next = old->next;
				Z_Free (old->matchvalue);
				Z_Free (old->message);
				Z_Free (old);
			}
		
			last->next = bans->next;
			Z_Free (bans->varname);
			Z_Free (bans);
			return true;
		}
		last = bans;
	}

	return false;
}

static void SV_DelCvarBan_f (void)
{
	char	*match;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Remove a cvar check.\n"
					"Syntax : delcvarban <cvarname>\n"
					"Example: delcvarban gl_modulate\n", LOG_GENERAL);
		return;
	}

	match = Cmd_Argv(1);

	if (DeleteVarBan (&cvarbans, match))
		Com_Printf ("cvarban '%s' removed.\n", LOG_GENERAL, match);
	else
    	Com_Printf ("cvarban '%s' not found.\n", LOG_GENERAL, match);
}

static void SV_DelUserinfoBan_f (void)
{
	char	*match;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Remove a userinfo check.\n"
					"Syntax : deluserinfoban <varname>\n"
					"Example: deluserinfoban rate\n", LOG_GENERAL);
		return;
	}

	match = Cmd_Argv(1);

	if (DeleteVarBan (&userinfobans, match))
		Com_Printf ("userinfoban '%s' removed.\n", LOG_GENERAL, match);
	else
    	Com_Printf ("userinfoban '%s' not found.\n", LOG_GENERAL, match);
}

static qboolean AddVarBan (varban_t *list, char *cvar, char *blocktype, char *iffound)
{
	banmatch_t	*match = NULL;
	varban_t	*bans = list;
	int			blockmethod;
	char		*bt;

	bt = blocktype;

	if (blocktype[0] == '!')
		blocktype++;

	if (blocktype[0] == '>' || blocktype[0] == '<' || blocktype[0] == '=')
	{
		if (!isdigit (blocktype[1]))
		{
			Com_Printf ("Error: digit must follow modifier '%c'\n", LOG_GENERAL, blocktype[0]);
			return false;
		}
	}
	else if (blocktype[0] == '~' || blocktype[0] == '=' || blocktype[0] == '#')
	{
		if (!blocktype[1])
		{
			Com_Printf ("Error: Missing value after '%c' modifier.\n", LOG_GENERAL, blocktype[0]);
			return false;
		}
	}
	else if (blocktype[0] == '*' && blocktype[1])
	{
		Com_Printf ("Error: No value allowed after * (* is not a wildcard match!)\n", LOG_GENERAL);
		return false;
	}

	//blocktype = Cmd_Argv(2);

	if (!Q_stricmp (iffound, "kick"))
	{
		blockmethod = CVARBAN_KICK;
	}
	else if (!Q_stricmp (iffound, "blackhole"))
	{
		blockmethod = CVARBAN_BLACKHOLE;
	}
	else if (!Q_stricmp (iffound, "log"))
	{
		blockmethod = CVARBAN_LOGONLY;
	}
	else if (!Q_stricmp (iffound, "message"))
	{
		blockmethod = CVARBAN_MESSAGE;
	}
	else if (!Q_stricmp (iffound, "stuff"))
	{
		blockmethod = CVARBAN_STUFF;
	}
	else if (!Q_stricmp (iffound, "exec"))
	{
		blockmethod = CVARBAN_EXEC;
	}
	else
	{
		Com_Printf ("Error: Unknown match action '%s'\n", LOG_GENERAL, iffound);
		return false;
	}

	while (bans->next)
	{
		bans = bans->next;
		if (!Q_stricmp (bans->varname, cvar))
		{
			match = &bans->match;
			while (match->next)
			{
				match = match->next;
			}
			break;
		}
	}

	if (!match)
	{
		bans->next = Z_TagMalloc (sizeof(varban_t), TAGMALLOC_CVARBANS);
		bans = bans->next;
		bans->next = NULL;
		bans->varname = CopyString (cvar, TAGMALLOC_CVARBANS);

		match = &bans->match;
	}

	match->next = Z_TagMalloc (sizeof(banmatch_t), TAGMALLOC_CVARBANS);
	match = match->next;

	match->next = NULL;
	match->matchvalue = CopyString (bt, TAGMALLOC_CVARBANS);
	match->message = CopyString (Cmd_Args2 (4), TAGMALLOC_CVARBANS);
	match->blockmethod = blockmethod;

	return true;
}

static void SV_AddCvarBan_f (void)
{
	char		*cvar, *blocktype, *iffound;

	if (Cmd_Argc() < 4)
	{
		Com_Printf ("Purpose: Add a simple check for a client cvar.\n"
					"Syntax : addcvarban <cvarname> <[!]((=|<|>)numvalue|[~|#]string|*)> <KICK|BLACKHOLE|LOG|MESSAGE|STUFF|EXEC> message\n"
					"Example: addcvarban gl_modulate >2 KICK Use a lower gl_modulate\n"
					"Example: addcvarban frkq2_bot * BLACKHOLE\n"
					"Example: addcvarban vid_ref sw KICK Software mode is not allowed\n"
					"Example: addcvarban timescale !=1 KICK Illegal timescale value!\n"
					"Example: addcvarban rate <5000 STUFF set rate 5000\n"
					"Example: addcvarban version ~R1Q2 KICK R1Q2 is an evil hacked client!\n"
					"Example: addcvarban version \"Q2 3.20 PPC\" KICK That version is not allowed\n"
					"WARNING: If the match string requires quotes it can not be added via rcon.\n", LOG_GENERAL);
		return;
	}

	cvar = Cmd_Argv(1);
	blocktype = Cmd_Argv(2);
	iffound = Cmd_Argv(3);

	if (!AddVarBan(&cvarbans, cvar, blocktype, iffound))
		return;

	if (sv.state)
		Com_Printf ("cvarban for '%s %s' added.\n", LOG_GENERAL, cvar, blocktype);
}

static void SV_AddUserinfoBan_f (void)
{
	char		*cvar, *blocktype, *iffound;

	if (Cmd_Argc() < 4)
	{
		Com_Printf ("Purpose: Add a simple check for a userinfo variable.\n"
			"Syntax : adduserinfoban <varname> <[!]((=|<|>)numvalue|[~|#]string|*)> <KICK|BLACKHOLE|LOG|MESSAGE|STUFF|EXEC> message\n"
					"Example: adduserinfoban rate >15000 KICK Your rate value is too high!\n"
					"Example: adduserinfoban name ~lamer KICK Lamers aren't welcome here.\n"
					"Example: adduserinfoban name !~[R1Q2] KICK This server is for R1Q2 clan members only!\n"
					"Example: adduserinfoban msg !=0 KICK You must use msg 0 on this server.\n"
					"Example: adduserinfoban spectator =1 KICK No spectators please!\n"
					"Example: adduserinfoban fov <90 KICK No zoom aliases allowed!\n"
					"WARNING: If the match string requires quotes it can not be added via rcon.\n", LOG_GENERAL);
		return;
	}

	cvar = Cmd_Argv(1);
	blocktype = Cmd_Argv(2);
	iffound = Cmd_Argv(3);

	if (!AddVarBan(&userinfobans, cvar, blocktype, iffound))
		return;

	if (sv.state)
		Com_Printf ("userinfoban for '%s %s' added.\n", LOG_GENERAL, cvar, blocktype);
}

//===============================================================

static void SV_Listholes_f (void)
{
	int index = 0;
	blackhole_t *hole = &blackholes;

	Com_Printf ("Current blackhole listing:\n", LOG_GENERAL);

	while (hole->next)
	{
		hole = hole->next;
		Com_Printf ("%d: %s/%d (%s)\n", LOG_GENERAL, ++index, NET_inet_ntoa (hole->ip), MaskBits (hole->mask), hole->reason);
	}
}

qboolean UnBlackhole (int index);
static void SV_Delhole_f (void)
{
	int			x;
	netadr_t	adr;

	if (Cmd_Argc() < 2) {
		Com_Printf ("Purpose: Remove a blackhole from the list.\n"
					"Syntax : delhole <index|ip>\n"
					"Example: delhole 1\n", LOG_GENERAL);
		return;
	}


	if (NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		blackhole_t *temp;

		temp = &blackholes;
		x = 1;

		while (temp->next)
		{
			temp = temp->next;

			if (*(uint32 *)adr.ip == temp->ip)
				goto remove;

			x++;
		}
		Com_Printf ("IP address %s not found in blackholes.\n", LOG_GENERAL, Cmd_Argv(1));
		return;
	}
	else
	{
		x = atoi(Cmd_Argv(1));
	
		if (x <= 0)
		{
			Com_Printf ("Invalid index %d\n", LOG_GENERAL, x);
			return;
		}
	}

remove:

	if (UnBlackhole (x))
		Com_Printf ("Blackhole %d removed.\n", LOG_GENERAL, x);
	else
		Com_Printf ("Error removing blackhole %d.\n", LOG_GENERAL, x);
}

static const char *cmdbanmethodnames[] =
{
	"message",
	"kick",
	"silent",
	"blackhole"
};

static void SV_AddCommandBan_f (void)
{
	int16				logmethod;
	int16				method;
	bannedcommands_t	*x;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Prevents any client from executing a given command.\n"
					"Syntax : addcommandban <commandname> [method] [logmethod]\n"
					"           method=message|kick|silent|blackhole (default message)\n"
					"           logmethod=log|silent (default log)\n"
					"Example: addcommandban god\n"
					"Example: addcommandban frkq2cmd kick silent\n"
					"Example: addcommandban invdrop silent\n"
					"Example: addcommandban invdrop silent silent\n", LOG_GENERAL);
		return;
	}

	x = &bannedcommands;

	while (x->next)
	{
		x = x->next;

		if (!strcmp (x->name, Cmd_Argv(1)))
		{
			Com_Printf ("Command '%s' is already blocked.\n", LOG_GENERAL|LOG_ERROR, x->name);
			return;
		}
	}

	if (!Q_stricmp (Cmd_Argv(2), "kick"))
		method = CMDBAN_KICK;
	else if (!Q_stricmp (Cmd_Argv(2), "silent"))
		method = CMDBAN_SILENT;
	else if (!Q_stricmp (Cmd_Argv(2), "blackhole"))
		method = CMDBAN_BLACKHOLE;
	else
		method = CMDBAN_MESSAGE;

	if (!Q_stricmp (Cmd_Argv(3), "silent"))
		logmethod = CMDBAN_LOG_SILENT;
	else
		logmethod = CMDBAN_LOG_MESSAGE;

	x->next = Z_TagMalloc (sizeof(*x), TAGMALLOC_CMDBANS);
	x = x->next;

	x->name = CopyString (Cmd_Argv(1), TAGMALLOC_CMDBANS);
	x->kickmethod = method;
	x->logmethod = logmethod;
	x->next = NULL;

	if (sv.state)
		Com_Printf ("Command '%s' is blocked from use with %s.\n", LOG_GENERAL, x->name, cmdbanmethodnames[x->kickmethod]);
}

static void SV_ListBannedCommands_f (void)
{
	bannedcommands_t *x;

	x = &bannedcommands;

	Com_Printf ("Banned commands:\n", LOG_GENERAL);
	while (x->next)
	{
		x = x->next;
		Com_Printf ("%s (%s)\n", LOG_GENERAL, x->name, cmdbanmethodnames[x->kickmethod]);
	}
}

static void SV_DelCommandBan_f (void)
{
	bannedcommands_t *last, *temp;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Allows any client to execute a previously banned command.\n"
					"Syntax : delcommandban <commandname>\n"
					"Example: delcommandban god\n", LOG_GENERAL);
		return;
	}


	last = temp = &bannedcommands;

	while (temp->next)
	{
		last = temp;
		temp = temp->next;

		if (!strcmp (temp->name, Cmd_Argv(1)))
		{
			// just copy the next over, don't care if it's null
			last->next = temp->next;

			Z_Free (temp->name);
			Z_Free (temp);

			Com_Printf ("Command '%s' is now allowed.\n", LOG_GENERAL, Cmd_Argv(1));
			return;
		}
	}

	Com_Printf ("Command '%s' is not blocked from use.\n", LOG_GENERAL, Cmd_Argv(1));
}

static void SV_AddServerAlias_f (void)
{
	linkedvaluelist_t	*alias;
	char				*text;

	if (Cmd_Argc() < 3)
	{
		Com_Printf ("Purpose: Add a server-side aliased command that stuffs clients.\n"
					"Syntax : addserveralias <name> <commandstring>\n"
					"Example: addserveralias cow \"set gender_auto 0;set skin cow/black\"\n", LOG_GENERAL);
		return;
	}

	alias = &serveraliases;

	text = StripQuotes (Cmd_Args2(2));

	while (alias->next)
	{
		alias = alias->next;
		if (!Q_stricmp (alias->name, Cmd_Argv(1)))
		{
			Z_Free (alias->value);
			goto replace;
		}
	}

	//FIXME: make better tag
	alias->next = Z_TagMalloc (sizeof(linkedvaluelist_t), TAGMALLOC_ALIAS);
	alias = alias->next;
	alias->next = NULL;
	alias->name = CopyString (Cmd_Argv(1), TAGMALLOC_ALIAS);

replace:

	alias->value = Z_TagMalloc ((int)strlen(text)+2, TAGMALLOC_ALIAS);
	strcpy (alias->value, text);
	strcat (alias->value, "\n");

	if (sv.state)
		Com_Printf ("'%s' added to server aliases.\n", LOG_GENERAL, alias->name);
}

static void SV_DelServerAlias_f (void)
{
	linkedvaluelist_t	*alias, *last;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Remove a server alias.\n"
					"Syntax : delserveralias <name>\n"
					"Example: delserveralias cow\n", LOG_GENERAL);
		return;
	}

	last = alias = &serveraliases;

	while (alias->next)
	{
		alias = alias->next;
		if (!Q_stricmp (alias->name, Cmd_Argv(1)))
		{
			last->next = alias->next;
			Com_Printf ("Server alias '%s' removed.\n", LOG_GENERAL, alias->name);
			Z_Free (alias->value);
			Z_Free (alias->name);
			Z_Free (alias);
			return;
		}
		last = alias;
	}

	Com_Printf ("Server alias '%s' not found.\n", LOG_GENERAL, Cmd_Argv(1));
}

static void SV_ListServerAliases_f (void)
{
	linkedvaluelist_t	*alias;

	alias = &serveraliases;

	Com_Printf ("Server aliases:\n", LOG_GENERAL);
	while (alias->next)
	{
		alias = alias->next;
		Com_Printf ("%s: %s", LOG_GENERAL, alias->name, alias->value);
	}
}

static void SV_Addhole_f (void)
{
	netadr_t	adr;
	int			method;
	int			mask;

	if (Cmd_Argc() < 4)
	{
		Com_Printf ("Purpose: Prevents a given IP from communicating with the server.\n"
					"Syntax : addhole <ip-address/mask> <SILENT|MESSAGE> <message>\n"
					"Example: addhole 192.168.0.1 MESSAGE You are not welcome on this server.\n"
					"Example: addhole 172.16.0.0/16 SILENT The clients will never see this text.\n", LOG_GENERAL);
		return;
	}

	if (!ValidateIPMask (Cmd_Argv(1), &adr, &mask))
		return;

	if (!Q_stricmp (Cmd_Argv(2), "SILENT"))
		method = BLACKHOLE_SILENT;
	else if (!Q_stricmp (Cmd_Argv(2), "MESSAGE"))
		method = BLACKHOLE_MESSAGE;
	else
	{
		Com_Printf ("Unknown blackhole action '%s'\n", LOG_GENERAL, Cmd_Argv(2));
		return;
	}

	Blackhole (&adr, false, mask, method, "%s", Cmd_Args2(3));
}

static qboolean ValidateAndAddToNetBlockList (char *ip, netblock_t *list, int tag)
{
	int			mask;
	netadr_t	from;
	netblock_t	*n;

	if (!ValidateIPMask (ip, &from, &mask))
		return false;

	n = list;
	while (n->next)
		n = n->next;

	n->next = Z_TagMalloc (sizeof(*n), tag);
	n = n->next;

	n->ip = NET_htonl (*(uint32 *)from.ip);
	n->mask = NET_htonl (CalcMask(mask));
	n->next = NULL;

	return true;
}

static qboolean ValidateAndRemoveFromNetBlockList (char *ip, netblock_t *list)
{
	int			mask;
	netadr_t	from;
	netblock_t	*n, *last;
	uint32		network_ip, network_mask;

	if (!ValidateIPMask (ip, &from, &mask))
		return -1;

	network_ip = NET_htonl (*(uint32 *)from.ip);
	network_mask = NET_htonl (CalcMask (mask));

	n = last = list;

	while (n->next)
	{
		n = n->next;

		if (n->ip == network_ip && n->mask == network_mask)
		{
			last->next = n->next;
			Z_Free (n);
			return 0;
		}
	}

	return -2;
}

static void SV_AddWhiteHole_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Purpose: Allow a given IP block to bypass a blackhole filter.\n"
					"Syntax : addwhitehole <ip-address/mask>\n"
					"Example: addwhitehole 192.168.0.1\n", LOG_GENERAL);
		return;
	}

	if (ValidateAndAddToNetBlockList (Cmd_Argv(1), &blackhole_exceptions, TAGMALLOC_BLACKHOLE))
	{
		if (sv.state)
			Com_Printf ("Blackhole exception added.\n", LOG_GENERAL);
	}
}

static void SV_DelWhiteHole_f (void)
{
	int	ret;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Purpose: Remove a blackhole bypass entry.\n"
					"Syntax : delwhitehole <ip-address/mask>\n"
					"Example: delwhitehole 192.168.0.1\n", LOG_GENERAL);
		return;
	}

	ret = ValidateAndRemoveFromNetBlockList (Cmd_Argv(1), &blackhole_exceptions);

	if (sv.state)
	{
		if (!ret)
			Com_Printf ("Blackhole exception removed.\n", LOG_GENERAL);
		else if (ret == -2)
			Com_Printf ("Entry not found.\n", LOG_GENERAL);
	}
}

static void SV_ListWhiteHoles_f (void)
{
	DumpNetBlockList (&blackhole_exceptions);
}

#ifdef ANTICHEAT
static void SV_AddACException_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Purpose: Allow a given IP block to bypass anticheat requirements.\n"
					"Syntax : addacexception <ip-address/mask>\n"
					"Example: addacexception 192.168.0.1\n", LOG_GENERAL);
		return;
	}

	if (ValidateAndAddToNetBlockList (Cmd_Argv(1), &anticheat_exceptions, TAGMALLOC_ANTICHEAT))
	{
		if (sv.state)
			Com_Printf ("Anticheat exception added.\n", LOG_GENERAL);
	}
}

static void SV_DelACException_f (void)
{
	int	ret;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Purpose: Remove a given IP block from the anticheat bypass list.\n"
					"Syntax : delacexception <ip-address/mask>\n"
					"Example: delacexception 192.168.0.1\n", LOG_GENERAL);
		return;
	}

	ret = ValidateAndRemoveFromNetBlockList (Cmd_Argv(1), &anticheat_exceptions);

	if (sv.state)
	{
		if (!ret)
			Com_Printf ("Anticheat exception removed.\n", LOG_GENERAL);
		else if (ret == -2)
			Com_Printf ("Entry not found.\n", LOG_GENERAL);
	}
}

static void SV_ListACExceptions_f (void)
{
	DumpNetBlockList (&anticheat_exceptions);
}

static void SV_AddACRequirement_f (void)
{
	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Purpose: Force a given IP block to require anticheat.\n"
					"Syntax : addacrequirement <ip-address/mask>\n"
					"Example: addacrequirement 192.168.0.1\n", LOG_GENERAL);
		return;
	}

	if (ValidateAndAddToNetBlockList (Cmd_Argv(1), &anticheat_requirements, TAGMALLOC_ANTICHEAT))
	{
		if (sv.state)
			Com_Printf ("Anticheat requirement added.\n", LOG_GENERAL);
	}
}

static void SV_DelACRequirement_f (void)
{
	int	ret;

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Purpose: Remove a given IP block from the anticheat requirement list.\n"
					"Syntax : delacrequirement <ip-address/mask>\n"
					"Example: delacrequirement 192.168.0.1\n", LOG_GENERAL);
		return;
	}

	ret = ValidateAndRemoveFromNetBlockList (Cmd_Argv(1), &anticheat_requirements);

	if (sv.state)
	{
		if (!ret)
			Com_Printf ("Anticheat requirement removed.\n", LOG_GENERAL);
		else if (ret == -2)
			Com_Printf ("Entry not found.\n", LOG_GENERAL);
	}
}

static void SV_ListACRequirements_f (void)
{
	DumpNetBlockList (&anticheat_requirements);
}
#endif

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
static void SV_Kick_f (void)
{
	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		//Com_Printf ("Usage: kick <userid>\n");
		Com_Printf ("Purpose: Kick a given id or player name from the server.\n"
					"Syntax : kick <userid>\n"
					"Example: kick 3\n", LOG_GENERAL);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	//r1: ignore kick message on connecting players (and those with no name)
	if (sv_client->state == cs_spawned && sv_client->name[0])
		SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", sv_client->name);
	// print directly, because the dropped client won't get the
	// SV_BroadcastPrintf message
	SV_ClientPrintf (sv_client, PRINT_HIGH, "You were kicked from the game\n");
	Com_Printf ("Dropping %s, console kick issued.\n", LOG_GENERAL|LOG_DROP, sv_client->name);
	SV_DropClient (sv_client, true);
	sv_client->lastmessage = svs.realtime;	// min case there is a funny zombie
}

static void SV_Stuff_f (void)
{
	char *s;

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	if (Cmd_Argc() < 3)
	{
		Com_Printf ("Purpose: stuff raw string to client.\n"
					"Syntax : stuff <userid> <cmdstring>\n"
					"Example: stuff 3 say hello\n", LOG_GENERAL);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	s = strchr (Cmd_Args(), ' ');

	if (s)
	{
		s++;

		MSG_BeginWriting (svc_stufftext);
		MSG_WriteString (s);
		SV_AddMessage (sv_client, true);

		Com_Printf ("Wrote '%s' to %s.\n", LOG_GENERAL, s, sv_client->name);
	}
}

static void SV_Stuffall_f (void)
{
	client_t	*cl;
	int			i;
	char		*s;

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	s = Cmd_Args();

	for (i=0,cl=svs.clients ; i<maxclients->intvalue; i++,cl++)
	{
		if (cl->state <= cs_zombie)
			continue;

		MSG_BeginWriting (svc_stufftext);
		MSG_WriteString (s);
		SV_AddMessage (cl, true);
	}

	Com_Printf ("Wrote '%s' to all clients.\n", LOG_GENERAL, s);
}

static void SV_AddStuffCmd_f (void)
{
	char	*s;
	char	*dst;

	if (Cmd_Argc() < 3)
	{
		Com_Printf ("Purpose: Add a command sent to a client at a chosen time.\n"
					"Syntax : addstuffcmd <when> <cmdstring>\n"
					"Example: addstuffcmd connect set allow_download 1\n"
					"Example: addstuffcmd begin say I have just entered the game!!\n", LOG_GENERAL);
		return;
	}

	s = strchr (Cmd_Args(), ' ');
	if (s)
	{
		s++;

		s = StripQuotes (s);

		if (!Q_stricmp (Cmd_Argv(1), "connect"))
			dst = svConnectStuffString;
		else
			dst = svBeginStuffString;

		if (strlen(dst) + strlen(s) + 2 >= sizeof(svConnectStuffString))
		{
			Com_Printf ("SV_AddStuff_f: No more space available for stuffed commands.\n", LOG_GENERAL);
			return;
		}

		strcat (dst, s);
		strcat (dst, "\n");

		if (sv.state)
			Com_Printf ("'%s' added to stuffed commands list.\n", LOG_GENERAL, s);
	}
}

static void SV_ListStuffedCommands_f (void)
{
	int		i;
	char	*s, *p;
	char	buff[sizeof(svConnectStuffString)];

	i = 0;

	strcpy (buff, svConnectStuffString);
	p = s = buff;

	s = strchr (s, '\n');
	Com_Printf ("On Connect:\n", LOG_GENERAL);
	while (s)
	{
		s[0] = 0;
		Com_Printf ("%d: %s\n", LOG_GENERAL, i, p);
		i++;
		s++;
		p = s;
		s = strchr (s, '\n');
	}

	i = 0;

	strcpy (buff, svBeginStuffString);
	p = s = buff;

	s = strchr (s, '\n');
	Com_Printf ("\nOn Begin:\n", LOG_GENERAL);
	while (s)
	{
		s[0] = 0;
		Com_Printf ("%d: %s\n", LOG_GENERAL, i, p);
		i++;
		s++;
		p = s;
		s = strchr (s, '\n');
	}
}

static void SV_DelStuffCmd_f (void)
{
	int		length;
	int		i;
	int		index;
	char	*dst, *p;

	if (Cmd_Argc() < 3)
	{
		Com_Printf ("Purpose: Remove a stuffed command.\n"
					"Syntax : delstuffcmd <when> <index>\n"
					"Example: delstuffcmd connect 1\n", LOG_GENERAL);
		return;
	}

	index = atoi(Cmd_Argv(2));

	if (!Q_stricmp (Cmd_Argv(1), "connect"))
		dst = svConnectStuffString;
	else
		dst = svBeginStuffString;

	i = 0;

	p = dst;

	dst = strchr (dst, '\n');
	while (dst)
	{
		if (i == index)
		{
			char	*s;

			s = strchr (p, '\n');
			s++;
			dst++;

			length = (int)strlen(dst);

			memmove (p, dst, length+1);
			Com_Printf ("Stuffcmd removed.\n", LOG_GENERAL);
			return;
		}
		i++;
		dst++;
		p = dst;
		dst = strchr (dst, '\n');
	}

	Com_Printf ("Index %d not found.\n", LOG_GENERAL, index);
}

static void SV_AddNullCmd_f (void)
{
	linkednamelist_t	*ncmd;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Make an otherwise undefined user command do nothing.\n"
					"Syntax : addnullcmd <command>\n"
					"Example: addnullcmd invdrop\n", LOG_GENERAL);
		return;
	}

	ncmd = &nullcmds;

	while (ncmd->next)
		ncmd = ncmd->next;

	//FIXME: make better tag
	ncmd->next = Z_TagMalloc (sizeof(linkednamelist_t), TAGMALLOC_CMDBANS);

	ncmd = ncmd->next;
	ncmd->name = CopyString (Cmd_Argv(1), TAGMALLOC_CMDBANS);
	ncmd->next = NULL;

	if (sv.state)
		Com_Printf ("'%s' nullified.\n", LOG_GENERAL, ncmd->name);
}

static void SV_DelNullCmd_f(void)
{
	char				*match;
	linkednamelist_t	*ncmd;
	linkednamelist_t	*last;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Remove a nullcmd.\n"
					"Syntax : delnullcmd <substring>\n"
					"Example: delnullcmd invdrop\n", LOG_GENERAL);
		return;
	}

	match = Cmd_Argv(1);

	ncmd = last = &nullcmds;

	while (ncmd->next)
	{
		ncmd = ncmd->next;
		if (!Q_stricmp (match, ncmd->name))
		{
			last->next = ncmd->next;
			Z_Free (ncmd->name);
			Z_Free (ncmd);

			Com_Printf ("nullcmd '%s' removed.\n", LOG_GENERAL, match);
			return;
		}
		last = ncmd;
	}

	Com_Printf ("%s not found.\n", LOG_GENERAL, match);
}

static void SV_AddLrconCmd_f (void)
{
	linkednamelist_t	*ncmd;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Allow a command to be used with limited rcon.\n"
					"Syntax : addlrconcmd <substring>\n"
					"Example: addlrconcmd set dmflags\n", LOG_GENERAL);
		return;
	}

	ncmd = &lrconcmds;

	while (ncmd->next)
		ncmd = ncmd->next;

	//FIXME: make better tag
	ncmd->next = Z_TagMalloc (sizeof(linkednamelist_t), TAGMALLOC_LRCON);

	ncmd = ncmd->next;
	ncmd->name = CopyString (StripQuotes(Cmd_Args()), TAGMALLOC_LRCON);
	ncmd->next = NULL;

	if (sv.state)
		Com_Printf ("'%s' added to lrcon commands.\n", LOG_GENERAL, ncmd->name);
}

static void SV_DelLrconCmd_f(void)
{
	int					mlen;
	char				*match;
	linkednamelist_t	*ncmd;
	linkednamelist_t	*last;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Remove a limited rcon command.\n"
					"Syntax : dellrconcmd <substring>\n"
					"Example: dellrconcmd set dmflags\n", LOG_GENERAL);
		return;
	}

	match = StripQuotes(Cmd_Args());
	mlen = (int)strlen(match);

	ncmd = last = &lrconcmds;

	while (ncmd->next)
	{
		ncmd = ncmd->next;
		if (!Q_strncasecmp (match, ncmd->name, mlen))
		{
			last->next = ncmd->next;
			Com_Printf ("lrcon command '%s' removed.\n", LOG_GENERAL, ncmd->name);
			Z_Free (ncmd->name);
			Z_Free (ncmd);
			return;
		}
		last = ncmd;
	}

	Com_Printf ("lrcon command '%s' not found.\n", LOG_GENERAL, match);
}

static void SV_DumpLinkedList (linkednamelist_t *list)
{
	while (list->next)
	{
		list = list->next;
		Com_Printf ("%s\n", LOG_GENERAL, list->name);
	}
}

static void SV_ListLrconCmds_f (void)
{
	Com_Printf ("Allowed lrcon commands:\n", LOG_GENERAL);
	SV_DumpLinkedList (&lrconcmds);
}

static void SV_ListNullCmds_f (void)
{
	Com_Printf ("Nullcmds:\n", LOG_GENERAL);
	SV_DumpLinkedList (&nullcmds);
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f (void)
{
	int			i;//, j, l;
	client_t	*cl;
	char		*s;
	int			ping;
	int			statusMethod;

//	player_state_new		*ps;
	//union player_state_t	*ps;

	if (!svs.clients)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}
	Com_Printf ("map              : %s\n", LOG_GENERAL, sv.name);

	statusMethod = atoi(Cmd_Args());

	if (statusMethod == 1)
	{
		Com_Printf (" # name            version string\n", LOG_GENERAL);
		Com_Printf ("-- --------------- -----------------------------------------------------\n", LOG_GENERAL);
	}
	else if (statusMethod == 2)
	{
		Com_Printf (" # name            file                                  position done z\n", LOG_GENERAL);
		Com_Printf ("-- --------------- ------------------------------------- -------- ---- -\n", LOG_GENERAL);
	}
	else if (statusMethod == 3)
	{
		Com_Printf (" # name            msglen overflow\n", LOG_GENERAL);
		Com_Printf ("-- --------------- ------ --------\n", LOG_GENERAL);
	}
	else
	{
		Com_Printf ("num score ping name            lastmsg ip address            rate/pps ver\n", LOG_GENERAL);
		Com_Printf ("--- ----- ---- --------------- ------- --------------------- -------- ---\n", LOG_GENERAL);
		//ideally want to keep this < 80 chars to avoid wrapping.
/*
 # score ping name            lastmsg address               rate/pps ver
-- ----- ---- --------------- ------- --------------------- -------- ---
 0     1   58 [PD]Monk              4 210.84.229.185:50132  3.2K/ 49  34
 */
	}

	for (i=0,cl=svs.clients ; i<maxclients->intvalue; i++,cl++)
	{
		if (!cl->state)
			continue;

		switch (statusMethod)
		{
			case 1:
				Com_Printf ("%2i %-15s %-54s\n", LOG_GENERAL, i, cl->name, cl->versionString);
				continue;
			case 2:
				if (cl->download)
					Com_Printf ("%2i %-15s %-37s %8d %3d%% %d\n", LOG_GENERAL, i, cl->name, cl->downloadFileName, cl->downloadcount, (int)(((float)cl->downloadcount / (float)cl->downloadsize)*100), cl->downloadCompressed);
				continue;
			case 3:
				Com_Printf ("%2i %-15s %-6d %.3f\n", LOG_GENERAL, i, cl->name, cl->netchan.message.buffsize, cl->commandMsecOverflowCount);
				continue;
			default:
				break;
		}

		/*if (ge->apiversion == GAME_API_VERSION_ENHANCED)
			ps = (player_state_new *)&cl->edict->client->ps.new_ps;
		else
			ps = (player_state_new *)&cl->edict->client->ps.old_ps;*/

#ifdef ENHANCED_SERVER
			Com_Printf ("%3i %5i ", LOG_GENERAL, i, ((struct gclient_new_s *)(cl->edict->client))->ps.stats[STAT_FRAGS]);
#else
			Com_Printf ("%3i %5i ", LOG_GENERAL, i, ((struct gclient_old_s *)(cl->edict->client))->ps.stats[STAT_FRAGS]);
#endif

		if (cl->state == cs_connected || cl->state == cs_spawning) {
			if (cl->download)
				Com_Printf ("DNLD ", LOG_GENERAL);
			else if (cl->downloadsize)
				Com_Printf ("HTTP ", LOG_GENERAL);
			else 
				Com_Printf ("CNCT ", LOG_GENERAL);
		} else if (cl->state == cs_zombie) {
			Com_Printf ("ZMBI ", LOG_GENERAL);
		} else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", LOG_GENERAL, ping);
		}

		s = NET_AdrToString (&cl->netchan.remote_address);

		Com_Printf ("%-15s %7i %-22s", LOG_GENERAL, cl->name, svs.realtime - cl->lastmessage, s);
		
		//r1: qport not so useful
		{
			float rateval = (float)cl->rate/1000.0f;
			if (rateval < 10)
				Com_Printf ("%.1fK/", LOG_GENERAL, rateval);
			else
				Com_Printf ("%.0fK /", LOG_GENERAL, rateval);
		}
		Com_Printf ("%3i %3i\n", LOG_GENERAL, cl->fps, cl->protocol);
	}
	Com_Printf ("\n", LOG_GENERAL);

#ifndef NPROFILE
	if (statusMethod == 1)
	{
		unsigned long	total;

		Com_Printf ("Protocol 35 netcode has saved %lu bytes.\n", LOG_GENERAL, svs.proto35BytesSaved);
		Com_Printf ("Protocol 35 compression has saved %lu bytes.\n", LOG_GENERAL, svs.proto35CompressionBytes);
		Com_Printf ("R1Q2 playerstate quantization optimization has saved %lu bytes.\n", LOG_GENERAL, svs.r1q2OptimizedBytes);
		Com_Printf ("R1Q2 entity quantization optimization has saved %lu bytes.\n", LOG_GENERAL, r1q2DeltaOptimizedBytes);
		Com_Printf ("R1Q2 custom delta management has saved %lu bytes.\n", LOG_GENERAL, svs.r1q2CustomBytes);
		Com_Printf ("R1Q2 sv_func_entities_hack has saved %lu bytes.\n", LOG_GENERAL, svs.r1q2AttnBytes);

		total = svs.proto35BytesSaved + svs.proto35CompressionBytes + svs.r1q2OptimizedBytes + svs.r1q2CustomBytes + r1q2DeltaOptimizedBytes + svs.r1q2AttnBytes;

		Com_Printf ("Total byte savings: %lu (%.2f MB)\n", LOG_GENERAL, total, (float)total / 1024.0 / 1024.0);
	}
#endif
}

/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];

	if (Cmd_Argc () < 2)
		return;

	if (!svs.initialized) {
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	strcpy (text, "console: ");
	p = StripQuotes(Cmd_Args());

	//r1: safety - use strncat
	strncat(text, p, 1014);

	//r1: also echo to console
	Com_Printf ("%s\n", LOG_GENERAL|LOG_CHAT, text);


	for (j = 0, client = svs.clients; j < maxclients->intvalue; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;
		SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
	}
}

#ifdef _WIN32
#ifdef DEDICATED_ONLY
static void SV_InstallService_f (void)
{

	if (Cmd_Argc() < 3)
	{
		//Com_Printf ("Usage: installservice servername commandline\n");
		Com_Printf ("Purpose: Install a Win32 service for a server.\n"
					"Syntax : installservice <servicename> <commandline>\n"
					"Example: installservice Q2DM +set maxclients 16 +map q2dm1\n", LOG_GENERAL);
		return;
	}

	Sys_InstallService (Cmd_Argv(1), Cmd_Args());
}

static void SV_DeleteService_f (void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Purpose: Remove a Win32 service for a server.\n"
					"Syntax : deleteservice <servicename>\n"
					"Example: deleteservice Q2DM\n", LOG_GENERAL);
		return;
	}

	Sys_DeleteService (Cmd_Args());
}
#endif

static void SV_Trayicon_f (void)
{
	if (Cmd_Argc() < 2)
	{
		//Com_Printf ("Usage: tray on|off\n");
		Com_Printf ("Purpose: Enable or disable minimize to notifcation area.\n"
					"Syntax : tray [on|off]\n"
					"Example: tray on\n", LOG_GENERAL);
		return;
	}

	if (!Q_stricmp (Cmd_Argv(1), "on"))
		Sys_EnableTray ();
	else
		Sys_DisableTray ();
}

static void SV_Minimize_f (void)
{
	Sys_Minimize();
}
#endif

static void SV_Broadcast_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];

	if (Cmd_Argc () < 2)
		return;

	strcpy (text, "server: ");
	p = StripQuotes(Cmd_Args());

	strcat(text, p);

	for (j = 0, client = svs.clients; j < maxclients->intvalue; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;
		SV_ClientPrintf(client, PRINT_CHAT, "========================\n%s\n========================\n", text);
	}
}

/*
==================
SV_Heartbeat_f
==================
*/
static void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999999;
}


/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
static void SV_Serverinfo_f (void)
{
	Com_Printf ("Server info settings:\n", LOG_GENERAL);
	Info_Print (Cvar_Serverinfo());
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f (void)
{
	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	if (Cmd_Argc() != 2)
	{
		//Com_Printf ("Usage: dumpuser <userid>\n");
		Com_Printf ("Purpose: Show a client's userinfo string and other information.\n"
					"Syntax : dumpuser <userid>\n"
					"Example: dumpuser 1\n", LOG_GENERAL);
		return;
	}

	if (!SV_SetPlayer ())
		return;

	Com_Printf ("userinfo\n", LOG_GENERAL);
	Com_Printf ("--------\n", LOG_GENERAL);
	Info_Print (sv_client->userinfo);

	Com_Printf ("\nmisc\n", LOG_GENERAL);
	Com_Printf ("----\n", LOG_GENERAL);
	Com_Printf ("version  %s\n", LOG_GENERAL, sv_client->versionString);
	Com_Printf ("protocol %d\n", LOG_GENERAL, sv_client->protocol);
	Com_Printf ("ofCount  %.2f\n", LOG_GENERAL, sv_client->commandMsecOverflowCount);
	Com_Printf ("ping     %d\n", LOG_GENERAL, sv_client->ping);
	Com_Printf ("fps      %d\n", LOG_GENERAL, sv_client->fps);
}


/*
==============
SV_ServerRecord_f

Begins server demo recording.  Every entity and every message will be
recorded, but no playerinfo will be stored.  Primarily for demo merging.
==============
*/
static void SV_ServerRecord_f (void)
{
	char	name[MAX_OSPATH];
	byte	buf_data[32768];
	sizebuf_t	buf;
	int		len;
	int		i;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Purpose: Record a serverdemo of all activity that takes place.\n"
					"Syntax : serverrecord <demoname>\n"
					"Example: serverrecord demo1\n", LOG_GENERAL);
		//Com_Printf ("serverrecord <demoname>\n");
		return;
	}

	if (svs.demofile)
	{
		Com_Printf ("Already recording.\n", LOG_GENERAL);
		return;
	}

	if (sv.state != ss_game)
	{
		Com_Printf ("You must be in a level to record.\n", LOG_GENERAL);
		return;
	}

	if (strstr (Cmd_Argv(1), "..") || strchr (Cmd_Argv(1), '/') || strchr (Cmd_Argv(1), '\\') )
	{
		Com_Printf ("Illegal filename.\n", LOG_GENERAL);
		return;
	}

	//
	// open the demo file
	//
	Com_sprintf (name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	FS_CreatePath (name);
	svs.demofile = fopen (name, "wb");
	if (!svs.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n", LOG_GENERAL);
		return;
	}

	Com_Printf ("recording to %s.\n", LOG_GENERAL, name);

	// setup a buffer to catch all multicasts
	SZ_Init (&svs.demo_multicast, svs.demo_multicast_buf, sizeof(svs.demo_multicast_buf));

	//
	// write a single giant fake message with all the startup info
	//
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
	// send the serverdata
	MSG_BeginWriting (svc_serverdata);
	MSG_WriteLong (PROTOCOL_ORIGINAL);
	MSG_WriteLong (svs.spawncount);
	// 2 means server demo
	MSG_WriteByte (2);	// demos are always attract loops
	MSG_WriteString (Cvar_VariableString ("gamedir"));
	MSG_WriteShort (-1);
	// send full levelname
	MSG_WriteString (sv.configstrings[CS_NAME]);
	MSG_EndWriting (&buf);

	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
		if (sv.configstrings[i][0])
		{
			MSG_BeginWriting (svc_configstring);
			MSG_WriteShort (i);
			MSG_WriteString (sv.configstrings[i]);
			MSG_EndWriting (&buf);
			if (buf.cursize + 67 >= buf.maxsize) {
				Com_Printf ("not enough buffer space available.\n", LOG_GENERAL);
				fclose (svs.demofile);
				return;
			}
		}

	// write it to the demo file
	Com_DPrintf ("signon message length: %i\n", buf.cursize);
	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, svs.demofile);
	fwrite (buf_data, buf.cursize, 1, svs.demofile);

	// the rest of the demo file will be individual frames
}


/*
==============
SV_ServerStop_f

Ends server demo recording
==============
*/
static void SV_ServerStop_f (void)
{
	int		len;

	if (!svs.demofile)
	{
		Com_Printf ("Not doing a serverrecord.\n", LOG_GENERAL);
		return;
	}

	len = ftell(svs.demofile);

	fclose (svs.demofile);
	svs.demofile = NULL;
	Com_Printf ("Recording completed, %d bytes written.\n", LOG_GENERAL, len);
}


/*
===============
SV_KillServer_f

Kick everyone off, possibly in preparation for a new game

===============
*/
static void SV_KillServer_f (void)
{
	if (!svs.initialized)
		return;

	if (Cmd_Argc() == 1)
		SV_Shutdown ("Server was killed.\n", false, false);
	else
		SV_Shutdown ("Server is restarting...\n", true, false);

	NET_Config ( NET_NONE );	// close network sockets
}

/*
===============
SV_ServerCommand_f

Let the game dll handle a command
===============
*/
static void SV_ServerCommand_f (void)
{
	if (!ge)
	{
		Com_Printf ("No game loaded.\n", LOG_GENERAL);
		return;
	}

	ge->ServerCommand();
}

static void SV_PassiveConnect_f (void)
{
	if (sv.state != ss_game)
	{
		Com_Printf ("No game running.\n", LOG_GENERAL);
	}
	else
	{
		if (Cmd_Argc() < 2)
		{
			//Com_Printf ("Usage: pc ip:port\n");
			Com_Printf ("Purpose: Initiate a passive connection to a listening client.\n"
						"Syntax : pc <listening-address>\n"
						"Example: pc 192.168.0.1:32000\n", LOG_GENERAL);
		}
		else
		{
			netadr_t addr;
			if (!(NET_StringToAdr (Cmd_Argv(1), &addr)))
			{
				Com_Printf ("Bad IP: %s\n", LOG_GENERAL, Cmd_Argv(1));
			}
			else
			{
				Netchan_OutOfBand (NS_SERVER, &addr, 15, (byte *)"passive_connect");
				Com_Printf ("passive_connect request sent to %s\n", LOG_GENERAL, NET_AdrToString (&addr));
			}
		}
	}
}

//===========================================================

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands (void)
{
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);

	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_AddCommand ("demomap", SV_DemoMap_f);
	Cmd_AddCommand ("gamemap", SV_GameMap_f);
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	if ( dedicated->intvalue )
	{
		Cmd_AddCommand ("say", SV_ConSay_f);
		Cmd_AddCommand ("broadcast", SV_Broadcast_f);
	}

	Cmd_AddCommand ("serverrecord", SV_ServerRecord_f);
	Cmd_AddCommand ("serverstop", SV_ServerStop_f);

	Cmd_AddCommand ("save", SV_Savegame_f);
	Cmd_AddCommand ("load", SV_Loadgame_f);

	Cmd_AddCommand ("killserver", SV_KillServer_f);

	Cmd_AddCommand ("addhole", SV_Addhole_f);
	Cmd_AddCommand ("delhole", SV_Delhole_f);
	Cmd_AddCommand ("listholes", SV_Listholes_f);

	Cmd_AddCommand ("addcommandban", SV_AddCommandBan_f);
	Cmd_AddCommand ("delcommandban", SV_DelCommandBan_f);
	Cmd_AddCommand ("listbannedcommands", SV_ListBannedCommands_f);

	Cmd_AddCommand ("addserveralias", SV_AddServerAlias_f);
	Cmd_AddCommand ("delserveralias", SV_DelServerAlias_f);
	Cmd_AddCommand ("listserveraliases", SV_ListServerAliases_f);

	Cmd_AddCommand ("addcvarban", SV_AddCvarBan_f);
	Cmd_AddCommand ("delcvarban", SV_DelCvarBan_f);
	Cmd_AddCommand ("listcvarbans", SV_ListCvarBans_f);

	Cmd_AddCommand ("adduserinfoban", SV_AddUserinfoBan_f);
	Cmd_AddCommand ("deluserinfoban", SV_DelUserinfoBan_f);
	Cmd_AddCommand ("listuserinfobans", SV_ListUserinfoBans_f);

	Cmd_AddCommand ("checkcvarbans", SV_CheckCvarBans_f);

	Cmd_AddCommand ("stuff", SV_Stuff_f);
	Cmd_AddCommand ("stuffall", SV_Stuffall_f);

	Cmd_AddCommand ("addstuffcmd", SV_AddStuffCmd_f);
	Cmd_AddCommand ("liststuffcmds", SV_ListStuffedCommands_f);
	Cmd_AddCommand ("delstuffcmd", SV_DelStuffCmd_f);

	Cmd_AddCommand ("addnullcmd", SV_AddNullCmd_f);
	Cmd_AddCommand ("listnullcmds", SV_ListNullCmds_f);
	Cmd_AddCommand ("delnullcmd", SV_DelNullCmd_f);

	Cmd_AddCommand ("addlrconcmd", SV_AddLrconCmd_f);
	Cmd_AddCommand ("listlrconcmds", SV_ListLrconCmds_f);
	Cmd_AddCommand ("dellrconcmd", SV_DelLrconCmd_f);

	Cmd_AddCommand ("addwhitehole", SV_AddWhiteHole_f);
	Cmd_AddCommand ("delwhitehole", SV_DelWhiteHole_f);
	Cmd_AddCommand ("listwhiteholes", SV_ListWhiteHoles_f);

	//r1: service support
#ifdef _WIN32
#ifdef DEDICATED_ONLY
	Cmd_AddCommand ("installservice", SV_InstallService_f);
	Cmd_AddCommand ("deleteservice", SV_DeleteService_f);
#endif

	Cmd_AddCommand ("tray", SV_Trayicon_f);
	Cmd_AddCommand ("minimize", SV_Minimize_f);
#endif

	Cmd_AddCommand ("sv", SV_ServerCommand_f);

	Cmd_AddCommand ("pc", SV_PassiveConnect_f);

#ifdef ANTICHEAT
	Cmd_AddCommand ("svaclist", SVCmd_SVACList_f);
	Cmd_AddCommand ("svacinfo", SVCmd_SVACInfo_f);

	Cmd_AddCommand ("addacexception", SV_AddACException_f);
	Cmd_AddCommand ("delacexception", SV_DelACException_f);
	Cmd_AddCommand ("listacexceptions", SV_ListACExceptions_f);

	Cmd_AddCommand ("addacrequirement", SV_AddACRequirement_f);
	Cmd_AddCommand ("delacrequirement", SV_DelACRequirement_f);
	Cmd_AddCommand ("listacrequirements", SV_ListACRequirements_f);

#endif
}
