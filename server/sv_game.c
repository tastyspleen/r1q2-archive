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
// sv_game.c -- interface to the game dll

#include "server.h"

game_export_t	*ge;


/*
===============
PF_Unicast

Sends the contents of the mutlicast buffer to a single client
===============
*/
void EXPORT PF_Unicast (edict_t *ent, qboolean reliable)
{
	int		p;
	client_t	*client;

	if (!ent)
		return;

	p = NUM_FOR_EDICT(ent);
	if (p < 1 || p > maxclients->intvalue)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: Unicast to illegal entity index %d.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, p);

		if (sv_gamedebug->intvalue >= 3)
			DEBUGBREAKPOINT;
		return;
	}

	client = svs.clients + (p-1);

	//r1: trap bad writes from game dll
	if (client->state <= cs_connected)
	{
		Com_Printf ("GAME ERROR: Attempted to write %s to disconnected client %d, ignored.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, svc_strings[MSG_GetType()], p-1);
		
		MSG_FreeData();
		return;
	}

	SV_AddMessage (client, reliable);
}


/*
===============
PF_dprintf

Debug print to server console
===============
*/
void EXPORT PF_dprintf (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	if (Q_vsnprintf (msg, sizeof(msg)-1, fmt, argptr) < 0)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: PF_dprintf: message overflow.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 2)
			DEBUGBREAKPOINT;
	}
	va_end (argptr);

	Com_Printf ("%s", LOG_GAME|LOG_DEBUG, msg);
}


/*
===============
PF_cprintf

Print to a single client
===============
*/
void EXPORT PF_cprintf (edict_t *ent, int level, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n, len;
	client_t	*client;

	if (ent)
	{
		n = NUM_FOR_EDICT(ent);
		if (n < 1 || n > maxclients->intvalue)
		{
			if (sv_gamedebug->intvalue)
				Com_Printf ("GAME WARNING: cprintf to a non-client entity %d, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, n);

			if (sv_gamedebug->intvalue >= 3)
				DEBUGBREAKPOINT;
			return;
		}

		client = svs.clients + (n-1);
		if (client->state != cs_spawned)
		{
			Com_Printf ("GAME ERROR: PF_cprintf to disconnected client %d, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, n-1);

			if (sv_gamedebug->intvalue >= 2)
				DEBUGBREAKPOINT;
			return;
		}
	}

	va_start (argptr,fmt);
	len = Q_vsnprintf (msg, sizeof (msg)-1, fmt, argptr);
	va_end (argptr);

	if (len < 0)
	{
		Com_Printf ("GAME ERROR: PF_cprintf: overflow.\n", LOG_SERVER|LOG_ERROR);

		if (sv_gamedebug->intvalue >= 2)
			DEBUGBREAKPOINT;
		return;
	}

	if (ent)
	{
		SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	}
	else
	{
		if (level == PRINT_CHAT)
			Com_Printf ("%s", LOG_GAME|LOG_CHAT, msg);
		else
			Com_Printf ("%s", LOG_GAME, msg);
	}
}


/*
===============
PF_centerprintf

centerprint to a single client
===============
*/
void EXPORT PF_centerprintf (edict_t *ent, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;
	client_t	*client;
	
	if (!ent)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: PF_centerprintf to NULL ent, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 3)
			DEBUGBREAKPOINT;
		return;
	}

	n = NUM_FOR_EDICT(ent);

	client = svs.clients + (n-1);
	if (client->state != cs_spawned)
	{
		Com_Printf ("GAME ERROR: PF_centerprintf to disconnected client %d, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, n-1);

		if (sv_gamedebug->intvalue >= 2)
			DEBUGBREAKPOINT;
		return;
	}
		
	if (n < 1 || n > maxclients->intvalue)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: PF_centerprintf to non-client entity %d, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, n);

		if (sv_gamedebug->intvalue >= 3)
			DEBUGBREAKPOINT;
		return;
	}

	va_start (argptr,fmt);
	if (Q_vsnprintf (msg, sizeof(msg)-1, fmt, argptr) < 0)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: PF_centerprintf message overflow.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 3)
			DEBUGBREAKPOINT;
	}
	va_end (argptr);

	MSG_BeginWriting (svc_centerprint);
	MSG_WriteString (msg);
	PF_Unicast (ent, true);
}


/*
===============
PF_error

Abort the server with a game error
===============
*/
void EXPORT PF_error (char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt, argptr);
	va_end (argptr);

	Com_Error (ERR_GAME, "Game Error: %s", msg);
}


/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
void EXPORT PF_setmodel (edict_t *ent, char *name)
{
	int		i;
	cmodel_t	*mod;

	if (!name)
		Com_Error (ERR_DROP, "PF_setmodel: NULL model name");

	if (!ent)
		Com_Error (ERR_DROP, "PF_setmodel: NULL entity");

	i = SV_ModelIndex (name);
		
//	ent->model = name;
	ent->s.modelindex = i;

// if it is an inline model, get the size information for it
	if (name[0] == '*')
	{
		mod = CM_InlineModel (name);
		VectorCopy (mod->mins, ent->mins);
		VectorCopy (mod->maxs, ent->maxs);
		SV_LinkEdict (ent);
	}

}

__inline char *SV_FixPlayerSkin (char *val, char *player_name)
{
	static char	fixed_skin[MAX_QPATH];

	Com_DPrintf ("PF_Configstring: Overriding malformed playerskin '%s' with '%s\\male/grunt'\n", LOG_SERVER|LOG_WARNING, val, player_name);

	Q_strncpy (fixed_skin, player_name, MAX_QPATH - 12);
	strcat (fixed_skin, "\\male/grunt");

	return fixed_skin;
}

/*
===============
PF_Configstring

===============
*/
void EXPORT PF_Configstring (int index, char *val)
{
	int length;

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring: bad index %i (data: %s)", index, val);

	if (!val)
		val = "";

	//r1: note, only checking up to maxclients. some mod authors are unaware of CS_GENERAL
	//and override playerskins with custom info, ugh :(
	if (val[0] && sv_validate_playerskins->intvalue && index >= CS_PLAYERSKINS && index < CS_PLAYERSKINS + maxclients->intvalue)
	{
		char	*p;
		char	*player_name;
		char	*model_name;
		char	*skin_name;
		int		i;
		char	pname[MAX_QPATH];

		Q_strncpy (pname, val, sizeof(pname)-1);

		player_name = pname;
		
		p = strchr (pname, '\\');

		if (!p)
		{
			val = SV_FixPlayerSkin (val, pname);
			goto fixed;
		}

		*p = 0;
		p++;
		model_name = p;
		if (!*model_name)
		{
			val = SV_FixPlayerSkin (val, player_name);
			goto fixed;
		}

		p = strchr (model_name+1, '/');
		if (!p)
			p = strchr (model_name+1, '\\');

		if (!p)
		{
			val = SV_FixPlayerSkin (val, player_name);
			goto fixed;
		}

		*p = 0;
		p++;
		skin_name = p;

		if (!*skin_name)
		{
			val = SV_FixPlayerSkin (val, player_name);
			goto fixed;
		}

		length = strlen (model_name);
		for (i = 0; i < length; i++)
		{
			if (!isalnum(model_name[i]) && model_name[i] != '_')
			{
				Com_DPrintf ("PF_Configstring: Illegal character '%c' in playerskin '%s'\n", LOG_SERVER|LOG_WARNING, model_name[i], val);
				val = SV_FixPlayerSkin (val, player_name);
				goto fixed;
			}
		}

		length = strlen (skin_name);
		for (i = 0; i < length; i++)
		{
			if (!isalnum(skin_name[i]) && skin_name[i] != '_')
			{
				Com_DPrintf ("PF_Configstring: Illegal character '%c' in playerskin '%s'\n", LOG_SERVER|LOG_WARNING, skin_name[i], val);
				val = SV_FixPlayerSkin (val, player_name);
				break;
			}
		}
	}

fixed:

	length = strlen(val);

	if (length > (sizeof(sv.configstrings[index])*(MAX_CONFIGSTRINGS-index))-1)
		Com_Error (ERR_DROP, "configstring: index %d, '%s': too long", index, val);
	else if (index != CS_STATUSBAR && length > sizeof(sv.configstrings[index])-1)
		Com_Printf ("WARNING: configstring %d ('%.32s...') spans more than one subscript.\n", LOG_SERVER|LOG_WARNING, index, MakePrintable(val));

	if (!strcmp (sv.configstrings[index], val))
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: Redundant update of configstring index %d (%s).\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, index, MakePrintable(val));

		if (sv_gamedebug->intvalue >= 3)
			DEBUGBREAKPOINT;

		return;
	}

	strcpy (sv.configstrings[index], val);

	// send the update to everyone
	if (sv.state != ss_loading)
	{
		//r1: why clear?
		//SZ_Clear (&sv.multicast);
		MSG_BeginWriting (svc_configstring);
		MSG_WriteShort (index);
		MSG_WriteString (val);
		SV_Multicast (NULL, MULTICAST_ALL_R);
	}
}



void EXPORT PF_WriteChar (int c)
{
	if (sv_gamedebug->intvalue)
	{
		if (c > 127 || c < -128)
		{
			Com_Printf ("GAME WARNING: Called gi.WriteChar (%d) which exceeds range for char.\n", LOG_WARNING|LOG_SERVER|LOG_GAMEDEBUG);
			if (sv_gamedebug->intvalue > 1)
				DEBUGBREAKPOINT;
		}
	}
	MSG_WriteChar (c);
}

void EXPORT PF_WriteByte (int c)
{
	if (sv_gamedebug->intvalue)
	{
		if (c > 255 || c < 0)
		{
			Com_Printf ("GAME WARNING: Called gi.WriteByte (%d) which exceeds range for byte.\n", LOG_WARNING|LOG_SERVER|LOG_GAMEDEBUG);
			if (sv_gamedebug->intvalue > 1)
				DEBUGBREAKPOINT;
		}
	}
	MSG_WriteByte (c & 0xFF);
}

void EXPORT PF_WriteShort (int c) {
	MSG_WriteShort (c);
}

void EXPORT PF_WriteLong (int c) {
	MSG_WriteLong (c);
}

void EXPORT PF_WriteFloat (float f) {
	MSG_WriteFloat (f);
}

void EXPORT PF_WriteString (char *s) {
	MSG_WriteString (s);
}

void EXPORT PF_WritePos (vec3_t pos) {
	MSG_WritePos (pos);
}

void EXPORT PF_WriteDir (vec3_t dir) {
	MSG_WriteDir (dir);
}

void EXPORT PF_WriteAngle (float f)
{
	MSG_WriteAngle (f);
}


/*
=================
PF_inPVS

Also checks portalareas so that doors block sight
=================
*/
qboolean EXPORT  PF_inPVS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPVS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);
	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks sight
	return true;
}


/*
=================
PF_inPHS

Also checks portalareas so that doors block sound
=================
*/
qboolean EXPORT PF_inPHS (vec3_t p1, vec3_t p2)
{
	int		leafnum;
	int		cluster;
	int		area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum (p1);
	cluster = CM_LeafCluster (leafnum);
	area1 = CM_LeafArea (leafnum);
	mask = CM_ClusterPHS (cluster);

	leafnum = CM_PointLeafnum (p2);
	cluster = CM_LeafCluster (leafnum);
	area2 = CM_LeafArea (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
		return false;		// more than one bounce away
	if (!CM_AreasConnected (area1, area2))
		return false;		// a door blocks hearing

	return true;
}

void EXPORT PF_StartSound (edict_t *entity, int channel, int sound_num, float volume,
    float attenuation, float timeofs)
{
	if (!entity)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: PF_StartSound: NULL entity, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 2)
			DEBUGBREAKPOINT;
		return;
	}
	SV_StartSound (NULL, entity, channel, sound_num, volume, attenuation, timeofs);
}

//==============================================

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_ShutdownGameProgs (void)
{
	if (!ge)
		return;
	ge->Shutdown ();
	Sys_UnloadGame ();
	ge = NULL;
}

/*
r1: pmove server wrapper.
any game DLL can supply a standard pmove_t which is then converted into an r1q2 enhanced
version (pmove_new_t) and passed onto pmove. think of it as a 'local version' of pmove - 
it allows the server to tag on extra data transparently to the game DLL, in this case,
the multiplier for clients running in the new protocol for fast spectator movement.
*/
void EXPORT SV_Pmove (pmove_t *pm)
{
	pmove_new_t epm;

	//transfer old pmove to new struct
	memcpy (&epm, pm, sizeof(pmove_t));

	//r1ch: allow non-client calls of this function
	if (sv_client && sv_client->protocol == ENHANCED_PROTOCOL_VERSION && pm->s.pm_type == PM_SPECTATOR)
		epm.multiplier = 2;
	else
		epm.multiplier = 1;

	epm.strafehack = (qboolean)sv_strafejump_hack->intvalue;

#ifdef ENHANCED_SERVER
	epm.enhanced = true;
#else
	epm.enhanced = false;
#endif

	//pmove
	Pmove (&epm);
	
	//copy results back out
	memcpy (pm, &epm, sizeof(pmove_t));
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
#ifdef DEDICATED_ONLY
void EXPORT SCR_DebugGraph (float value, int color)
{
}
#endif

void SV_InitGameProgs (void)
{
	edict_t			*ent;
	int				i;
	game_import_t	import;

	// unload anything we have now
	if (ge)
		SV_ShutdownGameProgs ();

	// load a new game dll
	import.multicast = SV_Multicast;
	import.unicast = PF_Unicast;
	import.bprintf = SV_BroadcastPrintf;
	import.dprintf = PF_dprintf;
	import.cprintf = PF_cprintf;
	import.centerprintf = PF_centerprintf;
	import.error = PF_error;

	import.linkentity = SV_LinkEdict;
	import.unlinkentity = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.trace = SV_Trace;
	import.pointcontents = SV_PointContents;
	import.setmodel = PF_setmodel;
	import.inPVS = PF_inPVS;
	import.inPHS = PF_inPHS;
	import.Pmove = SV_Pmove;

	import.modelindex = SV_ModelIndex;
	import.soundindex = SV_SoundIndex;
	import.imageindex = SV_ImageIndex;

	import.configstring = PF_Configstring;
	import.sound = PF_StartSound;
	import.positioned_sound = SV_StartSound;

	import.WriteChar = PF_WriteChar;
	import.WriteByte = PF_WriteByte;
	import.WriteShort = PF_WriteShort;
	import.WriteLong = PF_WriteLong;
	import.WriteFloat = PF_WriteFloat;
	import.WriteString = PF_WriteString;
	import.WritePosition = PF_WritePos;
	import.WriteDir = PF_WriteDir;
	import.WriteAngle = PF_WriteAngle;

	import.TagMalloc = Z_TagMallocGame;
	import.TagFree = Z_FreeGame;
	import.FreeTags = Z_FreeTagsGame;

	import.cvar = Cvar_GameGet;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;

	import.argc = Cmd_Argc;
	import.argv = Cmd_Argv;
	import.args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.DebugGraph = SCR_DebugGraph;
	import.SetAreaPortalState = CM_SetAreaPortalState;
	import.AreasConnected = CM_AreasConnected;

	ge = (game_export_t *)Sys_GetGameAPI (&import, sv.attractloop);

	if (!ge)
		Com_Error (ERR_DROP, "failed to load game DLL");

	i = ge->apiversion;

	if (i != GAME_API_VERSION)
	{
		//note, don't call usual unloadgame since that executes DLL code (which is invalid)
		Sys_UnloadGame ();
		ge = NULL;
		Com_Error (ERR_DROP, "Game is API version %i (not supported by this version R1Q2).", i);
		return;
	}

	Com_Printf ("Loaded Game DLL, version %d\n", LOG_SERVER, ge->apiversion);

	//r1: verify we got essential exports
	if (!ge->ClientCommand || !ge->ClientBegin || !ge->ClientConnect || !ge->ClientDisconnect || !ge->ClientUserinfoChanged ||
		!ge->ReadGame || !ge->ReadLevel || !ge->RunFrame || !ge->ServerCommand || !ge->Shutdown || !ge->SpawnEntities ||
		!ge->WriteGame || !ge->WriteLevel || !ge->Init)
		Com_Error (ERR_DROP, "Game is missing required exports");

	ge->Init ();
	if (!ge->edicts)
		Com_Error (ERR_DROP, "Game failed to initialize correctly");

	//r1: moved from SV_InitGame to here.
	for (i=0 ; i<maxclients->intvalue ; i++)
	{
		ent = EDICT_NUM(i+1);
		ent->s.number = i+1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
}
