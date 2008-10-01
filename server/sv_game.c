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
			Sys_DebugBreak ();
		return;
	}

	client = svs.clients + (p-1);

	//r1: trap bad writes from game dll
	if (client->state <= cs_spawning)
	{
		Com_Printf ("GAME ERROR: Attempted to write %d byte %s to disconnected client %d, ignored.\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG, MSG_GetLength(), svc_strings[MSG_GetType()], p-1);
		
		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();

		MSG_FreeData();
		return;
	}

	//r1: workaround some buggy mods methods of disconnecting players
	if (sv_disconnect_hack->intvalue)
	{
		if ((MSG_GetLength () == 1 && MSG_GetData()[0] == svc_disconnect) ||
			(MSG_GetLength () > 11 && MSG_GetData()[0] == svc_stufftext && !strncmp ((char *)MSG_GetData()+1, "disconnect\n", 11)))
		{
			MSG_FreeData ();
			Com_Printf ("Dropping %s, sv_disconnect_hack\n", LOG_SERVER, client->name);
			//don't drop now as game dll could reference this client later
			//SV_DropClient (client, true);
			client->notes |= NOTE_DROPME;
			return;
		}
	}
	SV_AddMessage (client, reliable);
}


/*
===============
PF_dprintf

Debug print to server console
===============
*/
void EXPORT PF_dprintf (const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	if (Q_vsnprintf (msg, sizeof(msg)-1, fmt, argptr) < 0)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: dprintf: message overflow.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();
	}
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	Com_Printf ("%s", LOG_GAME|LOG_DEBUG, msg);
}


/*
===============
PF_cprintf

Print to a single client
===============
*/
void EXPORT PF_cprintf (edict_t *ent, int level, const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			len;

	va_start (argptr,fmt);
	len = Q_vsnprintf (msg, sizeof (msg)-1, fmt, argptr);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	if (len < 0)
	{
		Com_Printf ("GAME ERROR: cprintf: message overflow.\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();
		return;
	}

	if (ent)
	{
		int			n;
		client_t	*client;

		n = NUM_FOR_EDICT(ent);

		if (n < 1 || n > maxclients->intvalue)
		{
			if (sv_gamedebug->intvalue)
				Com_Printf ("GAME WARNING: cprintf to a non-client entity %d, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, n);

			if (sv_gamedebug->intvalue >= 3)
				Sys_DebugBreak ();
			return;
		}

		client = svs.clients + (n-1);

		if (client->state != cs_spawned)
		{
			Com_Printf ("GAME ERROR: cprintf to disconnected client %d, ignored\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG, n-1);

			if (sv_gamedebug->intvalue >= 2)
				Sys_DebugBreak ();
			return;
		}

		SV_ClientPrintf (client, level, "%s", msg);
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
void EXPORT PF_centerprintf (edict_t *ent, const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;
	client_t	*client;
	
	if (!ent)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: centerprintf to NULL ent, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 3)
			Sys_DebugBreak ();
		return;
	}

	n = NUM_FOR_EDICT(ent);

	
	if (n < 1 || n > maxclients->intvalue)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: centerprintf to non-client entity %d, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, n);

		if (sv_gamedebug->intvalue >= 3)
			Sys_DebugBreak ();
		return;
	}

	client = svs.clients + (n-1);
	if (client->state != cs_spawned)
	{
		Com_Printf ("GAME ERROR: centerprintf to disconnected client %d, ignored\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG, n-1);

		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();
		return;
	}

	va_start (argptr,fmt);
	if (Q_vsnprintf (msg, sizeof(msg)-1, fmt, argptr) < 0)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: centerprintf message overflow.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 3)
			Sys_DebugBreak ();
	}
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

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
void EXPORT PF_error (const char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt, argptr);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	Com_Error (ERR_GAME, "Game Error: %s", msg);
}


/*
=================
PF_setmodel

Also sets mins and maxs for inline bmodels
=================
*/
void EXPORT PF_setmodel (edict_t *ent, const char *name)
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
		FastVectorCopy (mod->mins, ent->mins);
		FastVectorCopy (mod->maxs, ent->maxs);
		SV_LinkEdict (ent);
	}

}

__inline char *SV_FixPlayerSkin (char *val, char *player_name)
{
	static char	fixed_skin[MAX_QPATH];

	Com_DPrintf ("PF_Configstring: Overriding malformed playerskin '%s' with '%s\\male/grunt'\n", val, player_name);

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
	char	safestring[MAX_QPATH];
	size_t	length;
			

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring: bad index %i (data: %s)", index, MakePrintable(val, 0));

	if (!val)
		val = "";

	if (sv.state == ss_dead)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: configstring index %i (%s) set before server startup will not be saved.\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, index, MakePrintable(val, 0));

		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();
	}

	//r1: note, only checking up to maxclients. some mod authors are unaware of CS_GENERAL
	//and override playerskins with custom info, ugh :(
	if (val[0] && sv_validate_playerskins->intvalue && index >= CS_PLAYERSKINS && index < CS_PLAYERSKINS + maxclients->intvalue)
	{
		char	*p;
		char	*player_name;
		char	*model_name;
		char	*skin_name;
		char	pname[MAX_QPATH];
		size_t	i;

		Q_strncpy (pname, val, sizeof(pname)-1);

		player_name = pname;
		
		p = strchr (pname, '\\');

		if (!p)
		{
			val = SV_FixPlayerSkin (val, pname);
			goto fixed;
		}

		p[0] = 0;
		p++;
		model_name = p;
		if (!model_name[0])
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

		p[0] = 0;
		p++;
		skin_name = p;

		if (!skin_name[0])
		{
			val = SV_FixPlayerSkin (val, player_name);
			goto fixed;
		}

		length = strlen (model_name);
		for (i = 0; i < length; i++)
		{
			if (!isvalidchar(model_name[i]))
			{
				Com_DPrintf ("PF_Configstring: Illegal character '%c' in playerskin '%s'\n", model_name[i], val);
				val = SV_FixPlayerSkin (val, player_name);
				goto fixed;
			}
		}

		length = strlen (skin_name);
		for (i = 0; i < length; i++)
		{
			if (!isvalidchar(skin_name[i]))
			{
				Com_DPrintf ("PF_Configstring: Illegal character '%c' in playerskin '%s'\n", skin_name[i], val);
				val = SV_FixPlayerSkin (val, player_name);
				break;
			}
		}
	}

fixed:

	length = strlen(val);

	if (index > CS_MODELS && index < CS_GENERAL)
	{
		//r1: more configstring validation - some mods don't check the name\model/skin lengths and
		//this results in overwrite of CS_PLAYERSKINS subscripts.
		if (length > sizeof(sv.configstrings[index])-1)
		{
			Com_Printf ("GAME ERROR: configstring %d ('%.32s...') exceeds maximum allowed length, truncated.\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG, index, MakePrintable(val, 0));
			if (sv_gamedebug->intvalue > 1)
				Sys_DebugBreak ();
			Q_strncpy (safestring, val, sizeof(safestring)-1);
			val = safestring;
		}
	}
	else if (length > (sizeof(sv.configstrings[index])*(MAX_CONFIGSTRINGS-index))-1)
	{
		//way too long, bail out
		Com_Error (ERR_DROP, "configstring: index %d, '%s': too long", index, val);
	}
	else if (length > sizeof(sv.configstrings[index])-1)
	{
		//"harmless" overflow?
		if (index == CS_STATUSBAR)
		{
			//max allowed for statusbar space
			if (length > (sizeof(sv.configstrings[0]) * (CS_AIRACCEL-CS_STATUSBAR))-1)
				Com_Error (ERR_DROP, "CS_STATUSBAR configstring %d length %d exceeds maximum allowed length", index, (int)length);
		}
		else if (index > CS_STATUSBAR && index < CS_AIRACCEL)
		{
			//game dll is trying to set status bar one by one, WTF? just error out completely here.
			Com_Error (ERR_DROP, "CS_STATUSBAR configstring %d length %d exceeds maximum allowed length", index, (int)length);
		}
		else if (index == CS_NAME)
		{
			//some map names overflow this - i allow for one index overflow since
			//its only the cd audio that gets overwritten and seriously, how many maps set cd
			//audio to something other than 0?
			if (length > (sizeof(sv.configstrings[0]) * (CS_SKY-CS_NAME))-1)
			{
				Com_Error (ERR_DROP, "Map name exceeds maximum allowed length");
			}
			else if (length > sizeof(sv.configstrings[CS_NAME])-1)
			{
				Com_Printf ("WARNING: Map name exceeds maximum allowed length of 63 characters. R1Q2 will try to accomodate it anyway.\n", LOG_SERVER|LOG_WARNING);
			}
		}
		else
		{
			Com_Printf ("WARNING: configstring %d ('%.32s...') spans more than one subscript.\n", LOG_SERVER|LOG_WARNING, index, MakePrintable(val, 0));
		}
	}

	if (index == CS_CDTRACK)
	{
		int		track;

		track = atoi(val);

		//only accept this if its non-zero to allow for extended map name
		if (sv.configstrings[CS_CDTRACK][0])
		{
			if (!track && atoi(sv.configstrings[CS_CDTRACK]) == 0)
			{
				Com_Printf ("WARNING: Ignoring CS_CDTRACK to allow for extended map name length\n", LOG_SERVER|LOG_WARNING);
				index = -1;
				val = "";
			}
			else
			{
				//the map name overflowed and we have a valid cd track so lets terminate it
				sv.configstrings[CS_NAME][sizeof(sv.configstrings[CS_NAME])-1] = 0;
			}
		}
	}
	else if (index == CS_MAPCHECKSUM)
	{
		//shouldn't touch this!
		Com_Error (ERR_HARD, "Game DLL tried to set CS_MAPCHECKSUM");
	}
	else if (index == CS_SKYROTATE)
	{
		//optimize to save space
		sprintf (safestring, "%g", atof(val));
		val = safestring;
	}
	else if (index == CS_SKYAXIS)
	{
		//optimize to save space
		vec3_t	axis;
		if (sscanf (val, "%f %f %f", &axis[0], &axis[1], &axis[2]) != 3)
		{
			Com_Printf ("GAME ERROR: Invalid CS_SKYAXIS configstring '%s'!\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG, val);
			if (sv_gamedebug->intvalue > 1)
				Sys_DebugBreak ();
			VectorClear (axis);
		}
		val = safestring;
		sprintf (safestring, "%g %g %g", axis[0], axis[1], axis[2]);
	}

	if (!strcmp (sv.configstrings[index], val))
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: Redundant update of configstring index %d (%s).\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, index, MakePrintable(val, 0));

		if (sv_gamedebug->intvalue >= 3)
			Sys_DebugBreak ();

		return;
	}

	if (index != -1)
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
			Com_Printf ("GAME WARNING: Called gi.WriteChar (%d) which exceeds range for char.\n", LOG_WARNING|LOG_SERVER|LOG_GAMEDEBUG, c);
			if (sv_gamedebug->intvalue > 1)
				Sys_DebugBreak ();
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
			Com_Printf ("GAME WARNING: Called gi.WriteByte (%d) which exceeds range for byte.\n", LOG_WARNING|LOG_SERVER|LOG_GAMEDEBUG, c);
			if (sv_gamedebug->intvalue > 1)
				Sys_DebugBreak ();
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

void EXPORT PF_WriteString (const char *s) {
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
	vec3_t		neworigin;

	if (!entity)
	{
		if (sv_gamedebug->intvalue)
			Com_Printf ("GAME WARNING: PF_StartSound: NULL entity, ignored\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG);

		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();
		return;
	}

	if (sound_num >= MAX_SOUNDS || sound_num < 0)
	{
		Com_Printf ("GAME ERROR: PF_StartSound with illegal soundindex %d, ignored\n", LOG_SERVER|LOG_GAMEDEBUG|LOG_ERROR, sound_num);

		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();
		return;
	}

	channel &= ~CHAN_SERVER_ATTN_CALC;

	//r1: func_plat and friends trickery
	if (sv_func_plat_hack->intvalue)
	{
		if (entity->solid == SOLID_BSP && (channel & CHAN_NO_PHS_ADD))
		{
			const char	*s;
			int			type;

			type = 0;

			s = sv.configstrings[CS_SOUNDS + sound_num];

			//this is where the cvar gets its name :)
			if (!strcmp (s, "plats/pt1_strt.wav") || !strcmp (s, "plats/pt1_end.wav"))
				type = 1;
			else if (!strcmp (s, "doors/dr1_strt.wav") || !strcmp (s, "doors/dr1_end.wav"))
				type = 2;
			
			if (type)
			{
				//door or plat, do special attn. calculations serverside
				channel |= CHAN_SERVER_ATTN_CALC;
				channel &= ~CHAN_NO_PHS_ADD;
			
				//platform
				if (type == 1)
				{
					vec3_t	bounds;

					VectorSubtract (entity->maxs, entity->mins, bounds);

					neworigin[0] = entity->s.origin[0] + 0.5f * (entity->mins[0] + entity->maxs[0]);
					neworigin[1] = entity->s.origin[1] + 0.5f * (entity->mins[1] + entity->maxs[1]);
					neworigin[2] = entity->s.origin[2] + 0.5f * (entity->mins[2] + entity->maxs[2]);

					//r1: force the sound to the top of the actual platform (the bit you ride on)
					neworigin[2] += bounds[2] * 0.5f;

					SV_StartSound (neworigin, entity, channel, sound_num, volume, attenuation, timeofs);
					return;
				}
			}
		}
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

	Cvar_ForceSet ("g_features", "0");
	svs.game_features = 0;

	//r1: check all memory from game was cleaned.
	Z_CheckGameLeaks();
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
	client_t	*cl;

	//transfer old pmove to new struct
	memcpy (&epm, pm, sizeof(pmove_t));

	//temporarily snap all other players to their final move positions so player clipping doesn't get screwed up
	if (sv_interpolated_pmove->intvalue && epm.s.pm_type != PM_SPECTATOR)
	{
		for (cl = svs.clients; cl < svs.clients + maxclients->intvalue; cl++)
		{
			if (cl->state < cs_spawned || cl == sv_client)
				continue;

			if (cl->current_move.elapsed < cl->current_move.msec)
			{
				FastVectorCopy (cl->edict->s.origin, cl->current_move.origin_saved);
				FastVectorCopy (cl->current_move.origin_end, cl->edict->s.origin);
				SV_LinkEdict (cl->edict);
			}
		}
	}

	//r1ch: allow non-client calls of this function
	if (sv_client && sv_client->protocol == PROTOCOL_R1Q2)
	{
		if (pm->s.pm_type == PM_SPECTATOR)
			epm.multiplier = 2;

		if (sv_strafejump_hack->intvalue)
			epm.strafehack = true;
		else
			epm.strafehack = false;
	}
	else
	{
		epm.multiplier = 1;
		if (sv_strafejump_hack->intvalue == 2)
			epm.strafehack = true;
		else
			epm.strafehack = false;
	}

#ifdef ENHANCED_SERVER
	epm.enhanced = true;
#else
	epm.enhanced = false;
#endif

	//pmove
	Pmove (&epm);

	//copy results back out
	memcpy (pm, &epm, sizeof(pmove_t));

	//restore old partial moves
	if (sv_interpolated_pmove->intvalue && epm.s.pm_type != PM_SPECTATOR)
	{
		for (cl = svs.clients; cl < svs.clients + maxclients->intvalue; cl++)
		{
			if (cl->state < cs_spawned || cl == sv_client)
				continue;

			if (cl->current_move.elapsed < cl->current_move.msec)
			{
				FastVectorCopy (cl->current_move.origin_saved, cl->edict->s.origin);
				SV_LinkEdict (cl->edict);
			}
		}
	}
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
		Com_Error (ERR_HARD, "failed to load game DLL");

	i = ge->apiversion;

	if (i != GAME_API_VERSION)
	{
		//note, don't call usual unloadgame since that executes DLL code (which is invalid)
		Sys_UnloadGame ();
		ge = NULL;
		Com_Error (ERR_HARD, "Game is API version %i (not supported by this version R1Q2).", i);
		return;
	}

	Com_Printf ("Loaded Game DLL, version %d\n", LOG_SERVER, ge->apiversion);

	//r1: verify we got essential exports
	if (!ge->ClientCommand || !ge->ClientBegin || !ge->ClientConnect || !ge->ClientDisconnect || !ge->ClientUserinfoChanged ||
		!ge->ReadGame || !ge->ReadLevel || !ge->RunFrame || !ge->ServerCommand || !ge->Shutdown || !ge->SpawnEntities ||
		!ge->WriteGame || !ge->WriteLevel || !ge->Init)
		Com_Error (ERR_HARD, "Game is missing required exports");

	ge->Init ();
	if (!ge->edicts)
		Com_Error (ERR_HARD, "Game failed to initialize globals.edicts");

	if (g_features->intvalue)
	{
		svs.game_features = g_features->intvalue;
		Com_Printf ("Extended game features enabled:%s%s%s%s\n", LOG_SERVER,
			svs.game_features & GMF_CLIENTNUM ? " GMF_CLIENTNUM" : "", 
			svs.game_features & GMF_WANT_ALL_DISCONNECTS ? " GMF_WANT_ALL_DISCONNECTS" : "", 
			svs.game_features & GMF_PROPERINUSE ? " GMF_PROPERINUSE" : "", 
			svs.game_features & GMF_MVDSPEC ? " GMF_MVDSPEC" : "");
	}
	else
		svs.game_features = 0;

	memset (&svs.entities, 0, sizeof(svs.entities));

	//r1: moved from SV_InitGame to here.
	for (i=0 ; i<maxclients->intvalue ; i++)
	{
		ent = EDICT_NUM(i+1);
		ent->s.number = i+1;
		svs.clients[i].edict = ent;
		memset (&svs.clients[i].lastcmd, 0, sizeof(svs.clients[i].lastcmd));
	}
}
