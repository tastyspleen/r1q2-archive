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

#define	HEARTBEAT_SECONDS	300

netadr_t	master_adr[MAX_MASTERS];	// address of group servers

#ifdef USE_PYROADMIN
netadr_t	netaddress_pyroadmin;
#endif

client_t	*sv_client;			// current client

cvar_t	*sv_paused;
cvar_t	*sv_timedemo;
cvar_t	*sv_fpsflood;

cvar_t	*sv_enforcetime;

cvar_t	*timeout;				// seconds without any message
cvar_t	*zombietime;			// seconds to sink messages after disconnect

cvar_t	*rcon_password;			// password for remote server commands
cvar_t	*lrcon_password;

cvar_t	*sv_enhanced_setplayer;

cvar_t	*allow_download;
cvar_t	*allow_download_players;
cvar_t	*allow_download_models;
cvar_t	*allow_download_sounds;
cvar_t	*allow_download_maps;
cvar_t	*allow_download_pics;
cvar_t	*allow_download_textures;
cvar_t	*allow_download_others;

cvar_t *sv_airaccelerate;

cvar_t	*sv_noreload;			// don't reload level state when reentering

cvar_t	*maxclients;			// FIXME: rename sv_maxclients

#ifndef DEDICATED_ONLY
cvar_t	*sv_showclamp;
#endif

cvar_t	*hostname;
cvar_t	*public_server;			// should heartbeats be sent

#ifdef USE_PYROADMIN
cvar_t	*pyroadminport = &uninitialized_cvar;
#endif

cvar_t	*sv_locked;
cvar_t	*sv_restartmap;
cvar_t	*sv_password;

//r1: filtering of strings for non-ASCII
cvar_t	*sv_filter_q3names;
cvar_t	*sv_filter_userinfo;
cvar_t	*sv_filter_stringcmds;

//r1: stupid bhole code
cvar_t	*sv_blackholes;

//r1: allow server to ban people who use nodelta (uses 3-5x more bandwidth than regular client)
cvar_t	*sv_allownodelta;

//r1: allow server to block q2ace and associated hacks
cvar_t	*sv_deny_q2ace;

//r1: max connections from a single IP (prevent DoS)
cvar_t	*sv_iplimit;

//r1: allow server to send mini-motd
cvar_t	*sv_connectmessage;

//r1: for nocheat mods
cvar_t  *sv_nc_visibilitycheck;
cvar_t	*sv_nc_clientsonly;

//r1: max backup packets to allow from client
cvar_t	*sv_max_netdrop;

//r1: limit amount of info from status cmd
cvar_t	*sv_hidestatus;
cvar_t	*sv_hideplayers;

//r1: randomize server framenum
cvar_t	*sv_randomframe;

//r1: number of msecs to give
cvar_t	*sv_msecs;

cvar_t	*sv_nc_kick;
cvar_t	*sv_nc_announce;
cvar_t	*sv_filter_nocheat_spam ;

cvar_t	*sv_gamedebug;
cvar_t	*sv_recycle;

cvar_t	*sv_uptime;

cvar_t	*sv_strafejump_hack;
cvar_t	*sv_reserved_slots;
cvar_t	*sv_reserved_password;

cvar_t	*sv_allow_map;
cvar_t	*sv_allow_unconnected_cmds;

cvar_t	*sv_strict_userinfo_check;

cvar_t	*sv_calcpings_method;

cvar_t	*sv_no_game_serverinfo;

cvar_t	*sv_mapdownload_denied_message;
cvar_t	*sv_mapdownload_ok_message;

cvar_t	*sv_downloadserver;

cvar_t	*sv_max_traces_per_frame;

cvar_t	*sv_ratelimit_status;

cvar_t	*sv_new_entflags;

cvar_t	*sv_validate_playerskins;

cvar_t	*sv_idlekick;
cvar_t	*sv_packetentities_hack;
cvar_t	*sv_entity_inuse_hack;

cvar_t	*sv_force_reconnect;
cvar_t	*sv_download_refuselimit;

cvar_t	*sv_download_drop_file;
cvar_t	*sv_download_drop_message;

cvar_t	*sv_blackhole_mask;
cvar_t	*sv_badcvarcheck;

cvar_t	*sv_rcon_buffsize;
cvar_t	*sv_rcon_showoutput;

cvar_t	*sv_show_name_changes;

cvar_t	*sv_optimize_deltas;
cvar_t	*sv_advanced_deltas;

cvar_t	*sv_predict_on_lag;

#ifdef ANTICHEAT
cvar_t	*sv_require_anticheat;
cvar_t	*sv_anticheat_message;
#endif

//r1: not needed
//cvar_t	*sv_reconnect_limit;	// minimum seconds between connect messages

time_t	server_start_time;

blackhole_t			blackholes;
varban_t			cvarbans;
varban_t			userinfobans;
bannedcommands_t	bannedcommands;
linkednamelist_t	nullcmds;
linkednamelist_t	lrconcmds;
linkedvaluelist_t	serveraliases;

int		pyroadminid;

//void Master_Shutdown (void);

static int CharVerify (char c)
{
	if (sv_filter_q3names->intvalue == 2)
		return !(isalnum(c));
	else
		return (c == '^');
}

//============================================================================

static int NameColorFilterCheck (const char *newname)
{
	size_t	length;
	size_t	i;

	if (!sv_filter_q3names->intvalue)
		return 0;

	length = strlen (newname)-1;

	for (i = 0; i < length; i++)
	{
		if (CharVerify(*(newname+i)) && isdigit (*(newname+i+1)))	{
			return 1;
		}
	}

	return 0;
}

static int StringIsWhitespace (const char *name)
{
	const char	*p;

	p = name;

	while (*p)
	{
		if (!isspace (*p))
			return 0;
		p++;
	}

	return 1;
}

/*
=====================
SV_DropClient

Called when the player is totally leaving the server, either willingly
or unwillingly.  This is NOT called if the entire server is quiting
or crashing.
=====================
*/
void SV_DropClient (client_t *drop, qboolean notify)
{
	// add/force the disconnect
	if (notify)
	{
		MSG_BeginWriting (svc_disconnect);
		SV_AddMessage (drop, true);
	}
	else
	{
		//they did something naughty so they won't be seeing anything else from us...
		if (drop->messageListData)
		{
			SV_ClearMessageList (drop);
			Z_Free (drop->messageListData);
		}

		drop->messageListData = drop->msgListEnd = drop->msgListStart = NULL;
	}

	if (drop->state == cs_spawned)
	{
		// call the prog function for removing a client
		// this will remove the body, among other things
		ge->ClientDisconnect (drop->edict);
	}

	//r1: fix for mods that don't clean score
#ifdef ENHANCED_SERVER
	((struct gclient_new_s *)(drop->edict->client))->ps.stats[STAT_FRAGS] = 0;
#else
	((struct gclient_old_s *)(drop->edict->client))->ps.stats[STAT_FRAGS] = 0;
#endif

	drop->state = cs_zombie;
	drop->name[0] = 0;
}

void SV_KickClient (client_t *cl, const char /*@null@*/*reason, const char /*@null@*/*cprintf)
{
	if (reason && cl->state == cs_spawned && cl->name[0])
		SV_BroadcastPrintf (PRINT_HIGH, "%s was dropped: %s\n", cl->name, reason);
	if (cprintf)
		SV_ClientPrintf (cl, PRINT_HIGH, "%s", cprintf);
	Com_Printf ("Dropping %s, %s.\n", LOG_SERVER|LOG_DROP, cl->name, reason ? reason : "SV_KickClient");
	SV_DropClient (cl, true);
}

//r1: this does the final cleaning up of a client after zombie state.
static void SV_CleanClient (client_t *drop)
{
	//r1: drop message list
	if (drop->messageListData)
	{
		SV_ClearMessageList (drop);
		Z_Free (drop->messageListData);
	}

	//shouldn't be necessary, but...
	drop->messageListData = drop->msgListEnd = drop->msgListStart = NULL;

	//r1: free version string
	if (drop->versionString)
	{
		Z_Free (drop->versionString);
		drop->versionString = NULL;
	}

	//r1: download filename
	if (drop->downloadFileName)
	{
		Z_Free (drop->downloadFileName);
		drop->downloadFileName = NULL;
	}

	//r1: download data
	if (drop->download)
	{
		FS_FreeFile (drop->download);
		drop->download = NULL;
	}

	//r1: free baselines
	if (drop->lastlines)
	{
		Z_Free (drop->lastlines);
		drop->lastlines = NULL;
	}
}

const banmatch_t *SV_CheckUserinfoBans (char *userinfo, char *key)
{
	qboolean			waitForKey;
	char				myKey[MAX_INFO_VALUE];
	char				value[MAX_INFO_VALUE];
	const char			*s, *p;
	const banmatch_t	*match;

	if (!userinfobans.next)
		return NULL;

	s = userinfo;

	if (key[0])
		waitForKey = true;
	else
		waitForKey = false;

	while (s && *s)
	{
		s++;

		p = strchr (s, '\\');
		if (p)
		{
			Q_strncpy (myKey, s, p-s);
		}
		else
		{
			//uh oh...
			Com_Printf ("WARNING: Malformed userinfo string in SV_CheckUserinfoBans!\n", LOG_SERVER|LOG_WARNING);
			return NULL;
		}

		p++;
		s = strchr (p, '\\');
		if (s)
		{
			Q_strncpy (value, p, s-p);
		}
		else
		{
			Q_strncpy (value, p, MAX_INFO_VALUE-1);
		}

		if (waitForKey)
		{
			if (!strcmp (myKey, key))
				waitForKey = false;

			continue;
		}

		match = VarBanMatch (&userinfobans, myKey, value);

		if (match)
		{
			strcpy (key, myKey);
			return match;
		}
	}

	return NULL;
}

/*
==============================================================================

CONNECTIONLESS COMMANDS

==============================================================================
*/

/*
===============
SV_StatusString

Builds the string that is sent as heartbeats and status replies
===============
*/
static const char *SV_StatusString (void)
{
	char	*serverinfo;
	char	player[1024];
	static char	status[MAX_MSGLEN - 16];
	int		i;
	client_t	*cl;
	int		statusLength;
	int		playerLength;
//	player_state_new	*ps;

	serverinfo = Cvar_Serverinfo();

	//r1: add uptime info
	if (sv_uptime->intvalue)
	{
		char uptimeString[128];
		char tmpStr[16];

		uint32	secs;

		int days = 0;
		int hours = 0;
		int mins = 0;
		int	years = 0;

		uptimeString[0] = 0;
		secs = (uint32)(time(NULL) - server_start_time);

		while (secs/60/60/24/365 >= 1)
		{
			years++;
			secs -= 60*60*24*365;
		}

		while (secs/60/60/24 >= 1)
		{
			days++;
			secs -= 60*60*24;
		}

		while (secs/60/60 >= 1)
		{
			hours++;
			secs -= 60*60;
		}

		while (secs/60 >= 1)
		{
			mins++;
			secs -= 60;
		}

		if (years)
		{
			if (sv_uptime->intvalue == 1)
			{
				sprintf (tmpStr, "%d year%s", years, years == 1 ? "" : "s");
				strcat (uptimeString, tmpStr);
			}
			else
			{
				days += 365;
			}
		}

		if (days)
		{
			if (sv_uptime->intvalue == 1)
			{
				sprintf (tmpStr, "%dday%s", days, days == 1 ? "" : "s");
				if (uptimeString[0])
					strcat (uptimeString, ", ");
			}
			else
			{
				sprintf (tmpStr, "%d+", days);
			}
			strcat (uptimeString, tmpStr);
		}

		if (sv_uptime->intvalue == 1)
		{
			if (hours)
			{
				sprintf (tmpStr, "%dhr%s", hours, hours == 1 ? "" : "s");
				if (uptimeString[0])
					strcat (uptimeString, ", ");
				strcat (uptimeString, tmpStr);
			}
		}
		else
		{
			if (days || hours)
			{
				if (days)
					sprintf (tmpStr, "%.2d:", hours);
				else
					sprintf (tmpStr, "%d:", hours);
				strcat (uptimeString, tmpStr);
			}
		}

		if (sv_uptime->intvalue == 1)
		{
			if (mins)
			{
				sprintf (tmpStr, "%dmin%s", mins, mins == 1 ? "" : "s");
				if (uptimeString[0])
					strcat (uptimeString, ", ");
				strcat (uptimeString, tmpStr);
			}
			else if (!uptimeString[0])
			{
				sprintf (uptimeString, "%dsec%s", secs, secs == 1 ? "" : "s");
			}
		}
		else
		{
			if (days || hours || mins)
			{
				if (hours)
					sprintf (tmpStr, "%.2d.", mins);
				else
					sprintf (tmpStr, "%d.", mins);
				strcat (uptimeString, tmpStr);
			}
			if (days || hours || mins || secs)
			{
				if (mins)
					sprintf (tmpStr, "%.2d", secs);
				else
					sprintf (tmpStr, "%d", secs);
				strcat (uptimeString, tmpStr);
			}
		}

		Info_SetValueForKey (serverinfo, "uptime", uptimeString);
	}

	//r1: hide reserved slots
	Info_SetValueForKey (serverinfo, "maxclients", va("%d", maxclients->intvalue - sv_reserved_slots->intvalue));

	strcpy (status, serverinfo);
	strcat (status, "\n");
	statusLength = (int)strlen(status);

	if (!sv_hideplayers->intvalue)
	{
		for (i=0 ; i<(int)maxclients->intvalue ; i++)
		{
			cl = &svs.clients[i];
			if (cl->state >= cs_connected)
			{
	#ifdef ENHANCED_SERVER
					Com_sprintf (player, sizeof(player), "%i %i \"%s\"\n", 
						((struct gclient_new_s *)(cl->edict->client))->ps.stats[STAT_FRAGS], cl->ping, cl->name);
	#else
					Com_sprintf (player, sizeof(player), "%i %i \"%s\"\n", 
						((struct gclient_old_s *)(cl->edict->client))->ps.stats[STAT_FRAGS], cl->ping, cl->name);
	#endif
				playerLength = (int)strlen(player);
				if (statusLength + playerLength >= sizeof(status) )
					break;		// can't hold any more
				strcpy (status + statusLength, player);
				statusLength += playerLength;
			}
		}
	}

	return status;
}

static qboolean RateLimited (ratelimit_t *limit, int maxCount)
{
	int diff;

	diff = sv.time - limit->time;

	//a new sampling period
	if (diff > limit->period || diff < 0)
	{
		limit->time = sv.time;
		limit->count = 0;
	}
	else
	{
		if (limit->count >= maxCount)
			return true;
	}

	return false;
}

static void RateSample (ratelimit_t *limit)
{
	int diff;

	diff = sv.time - limit->time;

	//a new sampling period
	if (diff > limit->period || diff < 0)
	{
		limit->time = sv.time;
		limit->count = 1;
	}
	else
	{
		limit->count++;
	}
}

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see
================
*/
static void SVC_Status (void)
{
	if (sv_hidestatus->intvalue)
		return;

	RateSample (&svs.ratelimit_status);

	if (RateLimited (&svs.ratelimit_status, sv_ratelimit_status->intvalue))
	{
		Com_DPrintf ("SVC_Status: Dropped status request from %s\n", NET_AdrToString (&net_from));
		return;
	}

	Netchan_OutOfBandPrint (NS_SERVER, &net_from, "print\n%s", SV_StatusString());
}

/*
================
SVC_Ack

================
*/
static void SVC_Ack (void)
{
	int i;
	//r1: could be used to flood server console - only show acks from masters.

	for (i=0 ; i<MAX_MASTERS ; i++)
	{
		if (master_adr[i].port && NET_CompareBaseAdr (&master_adr[i], &net_from))
		{
			Com_Printf ("Ping acknowledge from %s\n", LOG_SERVER|LOG_NOTICE, NET_AdrToString(&net_from));
			break;
		}
	}
}

/*
================
SVC_Info

Responds with short info for broadcast scans
The second parameter should be the current protocol version number.
================
*/
static void SVC_Info (void)
{
	char	string[64];
	int		i, count;
	int		version;

	if (maxclients->intvalue == 1)
		return;		// ignore in single player

	version = atoi (Cmd_Argv(1));

	if (version != ORIGINAL_PROTOCOL_VERSION && version != ENHANCED_PROTOCOL_VERSION)
	{
		//r1: return instead of sending another packet. prevents spoofed udp packet
		//    causing server <-> server info loops.
		return;
	}
	else
	{
		count = 0;
		for (i=0 ; i<maxclients->intvalue ; i++)
			if (svs.clients[i].state >= cs_spawning)
				count++;

		Com_sprintf (string, sizeof(string), "%20s %8s %2i/%2i\n",
			hostname->string, sv.name, count, maxclients->intvalue - sv_reserved_slots->intvalue);
	}

	Netchan_OutOfBandPrint (NS_SERVER, &net_from, "info\n%s", string);
}

/*
================
SVC_Ping

Just responds with an acknowledgement
================
*/
static void SVC_Ping (void)
{
	Netchan_OutOfBandPrint (NS_SERVER, &net_from, "ack");
}


/*
=================
SVC_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/

static void SVC_GetChallenge (void)
{
	int		i;
	int		oldest = 0;
	uint32	oldestTime = 0xffffffff;

	// see if we already have a challenge for this ip
	for (i = 0 ; i < MAX_CHALLENGES ; i++)
	{
		if (NET_CompareBaseAdr (&net_from, &svs.challenges[i].adr))
			break;

		if (svs.challenges[i].time < oldestTime)
		{
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES)
	{
		// overwrite the oldest
		svs.challenges[oldest].challenge = randomMT()&0x7FFFFFFF;
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = curtime;
		i = oldest;
	} else {
		svs.challenges[i].challenge = randomMT()&0x7FFFFFFF;
		svs.challenges[i].time = curtime;
	}

	// send it back
	Netchan_OutOfBandPrint (NS_SERVER, &net_from, "challenge %d p=34,35", svs.challenges[i].challenge);
}

#if 0
static qboolean CheckUserInfoFields (char *userinfo)
{
	if (!Info_KeyExists (userinfo, "name")) /* || !Info_KeyExists (userinfo, "rate") 
		|| !Info_KeyExists (userinfo, "msg") || !Info_KeyExists (userinfo, "hand")
		|| !Info_KeyExists (userinfo, "skin"))*/
		return false;

	return true;
}
#endif

static qboolean SV_UserinfoValidate (const char *userinfo)
{
	const char		*s;
	const char		*p;

	s = userinfo;

	while (s && s[0])
	{
		//missing key separator
		if (s[0] != '\\')
			return false;

		s++;

		p = strchr (s, '\\');
		if (p)
		{
			//oversized key
			if (p-s >= MAX_INFO_KEY)
				return false;
		}
		else
		{
			//missing value separator
			return false;
		}

		p++;

		//missing value
		if (!p[0])
			return false;

		s = strchr (p, '\\');
		if (s)
		{
			//oversized value
			if (s-p >= MAX_INFO_VALUE)
				return false;
		}
		else
		{
			//oversized value at end of string
			if (strlen (p) >= MAX_INFO_VALUE)
				return false;
		}
	}

	return true;
}

static void SV_UpdateUserinfo (client_t *cl, qboolean notifyGame)
{
	char	*val;
	int		i;

	// call prog code to allow overrides
	if (notifyGame)
		ge->ClientUserinfoChanged (cl->edict, cl->userinfo);

	val = Info_ValueForKey (cl->userinfo, "name");

	if (val[0])
	{
		//truncate
		val[15] = 0;

		//r1: notify console
		if (cl->name[0] && val[0] && strcmp (cl->name, val))
		{
			Com_Printf ("%s[%s] changed name to %s.\n", LOG_SERVER|LOG_NAME, cl->name, NET_AdrToString (&cl->netchan.remote_address), val);
			if (sv_show_name_changes->intvalue)
				SV_BroadcastPrintf (PRINT_HIGH, "%s changed name to %s.\n", cl->name, val);
		}
		
		// name for C code
		strcpy (cl->name, val);

		// mask off high bit
		for (i=0 ; i<sizeof(cl->name)-1; i++)
			cl->name[i] &= 127;
	}
	else
	{
		cl->name[0] = 0;
	}

	// rate command
	val = Info_ValueForKey (cl->userinfo, "rate");
	if (val[0])
	{
		i = atoi(val);
		cl->rate = i;
		if (cl->rate < 100)
			cl->rate = 100;
		if (cl->rate > 15000)
			cl->rate = 15000;
	}
	else
		cl->rate = 5000;

	// msg command
	val = Info_ValueForKey (cl->userinfo, "msg");
	if (val[0])
	{
		cl->messagelevel = atoi(val);

		//safety check for integer overflow...
		if (cl->messagelevel < 0)
			cl->messagelevel = 0;
	}
}

/*
==================
SVC_DirectConnect

A connection request that did not come from the master
==================
*/
static void SVC_DirectConnect (void)
{
	netadr_t	*adr;

	client_t	*cl, *newcl;

	edict_t		*ent;

	//const banmatch_t	*match;

	int			i;
	unsigned	edictnum;
	unsigned	protocol;
	int			challenge;
	int			previousclients;

	qboolean	allowed;
	qboolean	reconnected;

	char		*pass;

	char		saved_var[32];
	char		saved_val[32];
	char		userinfo[MAX_INFO_STRING];
	//char		key[MAX_INFO_KEY];

	int			reserved;
	unsigned	msglen;

	uint16		qport;

	adr = &net_from;

	Com_DPrintf ("SVC_DirectConnect (%s)\n", NET_AdrToString (adr));

	//r1: check version first of all
	protocol= atoi(Cmd_Argv(1));
	if (protocol != ORIGINAL_PROTOCOL_VERSION && protocol != ENHANCED_PROTOCOL_VERSION)
	{
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nYou need Quake II 3.19 or higher to play on this server.\n");
		Com_DPrintf ("    rejected connect from protocol %i\n", protocol);
		return;
	}

	qport = atoi(Cmd_Argv(2));

	//work around older protocol 35 clients
	if (protocol == ENHANCED_PROTOCOL_VERSION)
	{
		if (qport > 0xFF)
			qport = 0;

		if (Cmd_Argv(5)[0])
		{
			msglen = atoi (Cmd_Argv(5));

			if (msglen > MAX_USABLEMSG)
			{
				Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nInvalid maximum message length.\n");
				Com_DPrintf ("    rejected msglen %u\n", msglen);
				return;
			}
			else if (msglen == 0)
			{
				msglen = MAX_USABLEMSG;
			}

			i = Cvar_IntValue ("net_maxmsglen");

			if (i && msglen > i)
				msglen = i;
		}
		else
		{
			msglen = 1390;
		}
	}
	else
	{
		msglen = 0;
	}

	challenge = atoi(Cmd_Argv(3));

	// see if the challenge is valid
	if (!NET_IsLocalHost (adr))
	{
		for (i=0 ; i<MAX_CHALLENGES ; i++)
		{
			if (svs.challenges[i].challenge && NET_CompareBaseAdr (adr, &svs.challenges[i].adr))
			{
				//r1: reset challenge
				if (challenge == svs.challenges[i].challenge)
				{
					svs.challenges[i].challenge = 0;
					break;
				}
				Com_DPrintf ("    bad challenge %i\n", challenge);
				Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nBad challenge.\n");
				return;
			}
		}
		if (i == MAX_CHALLENGES)
		{
			Com_DPrintf ("    no challenge\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nNo challenge for address.\n");
			return;
		}
	}

	//r1: deny if server is locked
	if (sv_locked->intvalue)
	{
		Com_DPrintf ("    server locked\n");
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nServer is locked.\n");
		return;
	}

	//r1: limit connections from a single IP
	previousclients = 0;
	for (i=0,cl=svs.clients ; i<maxclients->intvalue ; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;
		if (NET_CompareBaseAdr (adr, &cl->netchan.remote_address))
		{
			//r1: zombies are less dangerous
			if (cl->state == cs_zombie)
				previousclients++;
			else
				previousclients += 2;
		}
	}

	if (previousclients >= sv_iplimit->intvalue * 2)
	{
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nToo many connections from your host.\n");
		Com_DPrintf ("    too many connections\n");
		return;
	}

	Q_strncpy (userinfo, Cmd_Argv(4), sizeof(userinfo)-1);

	//r1: check it is not overflowed, save enough bytes for /ip/111.222.333.444:55555
	if (strlen(userinfo) + 25 >= sizeof(userinfo)-1)
	{
		Com_DPrintf ("    userinfo length exceeded\n");
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nUserinfo string length exceeded.\n");
		return;
	}
	else if (!userinfo[0])
	{
		Com_DPrintf ("    empty userinfo string\n");
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nBad userinfo string.\n");
		return;
	}

	if (!SV_UserinfoValidate (userinfo))
	{
		//Com_Printf ("WARNING: SV_UserinfoValidate failed for %s (%s)\n", LOG_SERVER|LOG_WARNING, NET_AdrToString (adr), MakePrintable (userinfo));
		Com_Printf ("EXPLOIT: Client %s supplied an illegal userinfo string: %s\n", LOG_EXPLOIT|LOG_SERVER, NET_AdrToString(adr), MakePrintable (userinfo, 0));
		Blackhole (adr, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "illegal userinfo string");
		return;
	}

	//r1ch: ban anyone trying to use the end-of-message-in-string exploit
	if (strchr(userinfo, '\xFF'))
	{
		char		*ptr;
		const char	*p;
		ptr = strchr (userinfo, '\xFF');
		ptr -= 8;
		if (ptr < userinfo)
			ptr = userinfo;
		p = MakePrintable (ptr, 0);
		Com_Printf ("EXPLOIT: Client %s supplied userinfo string containing 0xFF: %s\n", LOG_EXPLOIT|LOG_SERVER, NET_AdrToString(adr), p);
		Blackhole (adr, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "0xFF in userinfo (%.32s)", p);
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nConnection refused.\n");
		return;
	}

	if (sv_strict_userinfo_check->intvalue)
	{
		if (!Info_CheckBytes (userinfo))
		{
			Com_Printf ("Warning, Info_CheckBytes failed for %s!\n", LOG_SERVER|LOG_WARNING, NET_AdrToString (adr));
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nUserinfo contains illegal bytes.\n");
			return;
		}
	}

	//r1ch: allow filtering of stupid "fun" names etc.
	if (sv_filter_userinfo->intvalue)
		StripHighBits(userinfo, (int)sv_filter_userinfo->intvalue == 2);

	//r1ch: simple userinfo validation
	/*if (!CheckUserInfoFields(userinfo)) {
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nMalformed userinfo string.\n");
		Com_DPrintf ("illegal userinfo from %s\n", NET_AdrToString (adr));
		return;
	}*/

	/*key[0] = 0;
	while ((match = SV_CheckUserinfoBans (userinfo, key)))
	{
		if (match->message[0])
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\n%s\n", match->message);

		if (match->blockmethod == CVARBAN_LOGONLY)
		{
			Com_Printf ("LOG: %s[%s] matched userinfoban: %s == %s\n", LOG_SERVER, Info_ValueForKey (userinfo, "name"), NET_AdrToString(adr), key, Info_ValueForKey (userinfo, key));
		}
		else
		{
			if (match->blockmethod == CVARBAN_BLACKHOLE)
				Blackhole (adr, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "userinfovarban: %s == %s", key, Info_ValueForKey (userinfo, key));

			Com_DPrintf ("    userinfo ban %s matched\n", key);
			return;
		}
	}*/

	pass = Info_ValueForKey (userinfo, "name");

	if (!pass[0] || StringIsWhitespace (pass))
	{
		Com_DPrintf ("    missing name\n");
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nPlease set your name before connecting.\n");
		return;
	}
	else if (NameColorFilterCheck (pass))
	{
		Com_DPrintf ("    color name filter check failed\n");
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nQuake 3 style colored names are not permitted on this server.\n");
		return;
	}
	else if (Info_KeyExists (userinfo, "ip"))
	{
		char	*p;
		p = Info_ValueForKey(userinfo, "ip");
		Com_Printf ("EXPLOIT: Client %s[%s] attempted to spoof IP address: %s\n", LOG_EXPLOIT|LOG_SERVER, Info_ValueForKey (userinfo, "name"), NET_AdrToString(adr), p);
		Blackhole (adr, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "attempted to spoof ip '%s'", p);
		return;
	}

	pass = Info_ValueForKey (userinfo, "password");

	if (sv_password->string[0] && strcmp(sv_password->string, "none"))
	{
		if (!pass[0])
		{
			Com_DPrintf ("    empty password\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nPassword required.\n");
			return;
		}
		else if (strcmp (pass, sv_password->string))
		{
			Com_DPrintf ("    bad password\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nInvalid password.\n");
			return;
		}
	}

	// force the IP key/value pair so the game can filter based on ip
	Info_SetValueForKey (userinfo, "ip", NET_AdrToString(adr));

	// attractloop servers are ONLY for local clients
	// r1: demo serving anyone?
	/*if (sv.attractloop)
	{
		if (!NET_IsLocalAddress (adr))
		{
			Com_Printf ("Remote connect in attract loop.  Ignored.\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nConnection refused.\n");
			return;
		}
	}*/

	if (!strcmp (pass, sv_reserved_password->string))
	{
		reserved = 0;

		//r1: this prevents mod/admin dll from also checking password as some mods incorrectly
		//refuse if password cvar exists and no password is set on the server. by definition a
		//server with reserved slots should be public anyway.
		Info_RemoveKey (userinfo, "password");
	}
	else
	{
		reserved = sv_reserved_slots->intvalue;
	}

	//newcl = &temp;
//	memset (newcl, 0, sizeof(client_t));

	saved_var[0] = saved_val[0] = 0;

	// if there is already a slot for this ip, reuse it
	for (i=0,cl=svs.clients ; i < maxclients->intvalue; i++,cl++)
	{
		if (cl->state == cs_free)
			continue;

		if (NET_CompareAdr (adr, &cl->netchan.remote_address))
		{
			//r1: !! fix nasty bug where non-disconnected clients (from dropped disconnect
			//packets) could be overwritten!
			if (cl->state != cs_zombie)
			{
				Com_DPrintf ("    client already found\n");
				Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nPlayer '%s' is already connected from %s.\n", cl->name, NET_AdrToString(adr));
				return;
			}

			//r1: not needed
			/*if (!NET_IsLocalAddress (adr) && (svs.realtime - cl->lastconnect) < ((int)sv_reconnect_limit->value * 1000))
			{
				Com_DPrintf ("%s:reconnect rejected : too soon\n", NET_AdrToString (adr));
				return;
			}*/
			Com_DPrintf ("%s:reconnect\n", NET_AdrToString (adr));

			//r1: clean up last client data
			SV_CleanClient (cl);

			if (cl->reconnect_var[0])
			{
				if (adr->port != cl->netchan.remote_address.port)
					Com_Printf ("Warning, %s[%s] reconnected from a different port! Tried to use ratbot/proxy or has broken router?\n", LOG_SERVER|LOG_WARNING, Info_ValueForKey (userinfo, "name"), NET_AdrToString (adr));

				if (cl->protocol != protocol)
					Com_Printf ("Warning, %s[%s] reconnected using a different protocol! Why would that happen?\n", LOG_SERVER|LOG_WARNING, Info_ValueForKey (userinfo, "name"), NET_AdrToString (adr));
			}

			strcpy (saved_var, cl->reconnect_var);
			strcpy (saved_val, cl->reconnect_value);

			newcl = cl;
			goto gotnewcl;
		}
	}

	// find a client slot
	newcl = NULL;
	for (i=0,cl=svs.clients ; i<(maxclients->intvalue - reserved); i++,cl++)
	{
		if (cl->state == cs_free)
		{
			newcl = cl;
			break;
		}
	}

	if (!newcl)
	{
		if (!reserved)
		{
			Com_DPrintf ("    reserved slots full\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nServer and reserved slots are full.\n");
		}
		else
		{
			Com_DPrintf ("    server full\n");
			Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nServer is full.\n");
		}
		return;
	}

gotnewcl:	
	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	
	sv_client = newcl;

	if (newcl->messageListData || newcl->versionString || newcl->downloadFileName || newcl->download || newcl->lastlines)
	{
		Com_Printf ("WARNING: Client %d never got cleaned up, possible memory leak.\n", LOG_SERVER|LOG_WARNING, (int)(newcl - svs.clients));
	}

	memset (newcl, 0, sizeof(*newcl));

	//r1: reconnect check
	if (saved_var[0] && saved_val[0])
	{
		strcpy (newcl->reconnect_value, saved_val);
		strcpy (newcl->reconnect_var, saved_var);
	}

	edictnum = (int)(newcl-svs.clients)+1;
	ent = EDICT_NUM(edictnum);
	newcl->edict = ent;
	newcl->challenge = challenge; // save challenge for checksumming

	reconnected = (!sv_force_reconnect->string[0] || saved_var[0] || NET_IsLANAddress(adr));

	if (reconnected)
	{
		//if (!ge->edicts[0].client)
		//	Com_Error (ERR_DROP, "Missed a call to InitGame");
		// get the game a chance to reject this connection or modify the userinfo
		allowed = ge->ClientConnect (ent, userinfo);

		if (userinfo[MAX_INFO_STRING-1])
		{
			//probably already crashed by now but worth a try
			Com_Error (ERR_FATAL, "Userinfo string length overflowed after ClientConnect");
		}

		if (!allowed)
		{
			if (Info_ValueForKey (userinfo, "rejmsg")[0]) 
				Netchan_OutOfBandPrint (NS_SERVER, adr, "print\n%s\nConnection refused.\n",  
					Info_ValueForKey (userinfo, "rejmsg"));
			else
				Netchan_OutOfBandPrint (NS_SERVER, adr, "print\nConnection refused.\n" );
			Com_DPrintf ("    game rejected the connection.\n");
			return;
		}

		//did the game ruin our userinfo string?
		if (!userinfo[0])
		{
			Com_Printf ("GAME ERROR: Userinfo string corrupted after ClientConnect\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG); 
			if (sv_gamedebug->intvalue > 1)
				Sys_DebugBreak ();
		}
	}

	//moved netchan to here so userinfo changes can see remote address
	Netchan_Setup (NS_SERVER, &newcl->netchan, adr, protocol, qport, msglen);

	// parse some info from the info strings
	strcpy (newcl->userinfo, userinfo);

	//r1: this fills in the fields of userinfo.
	SV_UpdateUserinfo (newcl, reconnected);

	//r1: netchan init was here

	// send the connect packet to the client
	// r1: note we could ideally send this twice but it prints unsightly message on original client.
	if (sv_downloadserver->string[0])
		Netchan_OutOfBandPrint (NS_SERVER, adr, "client_connect dlserver=%s", sv_downloadserver->string);
	else
		Netchan_OutOfBandPrint (NS_SERVER, adr, "client_connect");

	if (sv_connectmessage->modified)
	{
		ExpandNewLines (sv_connectmessage->string);
		if (strlen(sv_connectmessage->string) >= MAX_USABLEMSG-16)
		{
			Com_Printf ("WARNING: sv_connectmessage string is too long!\n", LOG_SERVER|LOG_WARNING);
			Cvar_Set ("sv_connectmessage", "");
		}
		sv_connectmessage->modified = false;
	}

	//only show message on reconnect
	if (reconnected && sv_connectmessage->string[0])
		Netchan_OutOfBandPrint (NS_SERVER, adr, "print\n%s\n", sv_connectmessage->string);

	newcl->protocol = protocol;
	newcl->state = cs_connected;

	newcl->messageListData = Z_TagMalloc (sizeof(messagelist_t) * MAX_MESSAGES_PER_LIST, TAGMALLOC_CL_MESSAGES);
	memset (newcl->messageListData, 0, sizeof(messagelist_t) * MAX_MESSAGES_PER_LIST);

	newcl->msgListEnd = newcl->msgListStart = newcl->messageListData;
	newcl->msgListStart->data = NULL;

	newcl->lastlines = Z_TagMalloc (sizeof(entity_state_t) * MAX_EDICTS, TAGMALLOC_CL_BASELINES);
	//memset (newcl->lastlines, 0, sizeof(entity_state_t) * MAX_EDICTS);

	//r1: per client baselines are now used
	//SV_CreateBaseline (newcl);
	
	//r1: concept of datagram buffer no longer exists
	//SZ_Init (&newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf) );
	//newcl->datagram.allowoverflow = true;

	//r1ch: give them 5 secs to respond to the client_connect. since client_connect isn't
	//sent via netchan (its connectionless therefore unreliable), a dropped client_connect
	//will force a client to wait for the entire timeout->value before being allowed to try
	//and reconnect.
	//newcl->lastmessage = svs.realtime;	// don't timeout

	newcl->lastmessage = svs.realtime - ((timeout->intvalue - 5) * 1000);
}

static int Rcon_Validate (void)
{
	if (rcon_password->string[0] && !strcmp (Cmd_Argv(1), rcon_password->string) )
		return 1;
	else if (lrcon_password->string[0] && !strcmp (Cmd_Argv(1), lrcon_password->string))
		return 2;

	return 0;
}

static uint32 CalcMask (int32 bits)
{
	int				i;
	uint32			mask;

	mask = 0xFFFFFFFF;

	for (i = 0; i < 32; i++)
	{
		if (i >= bits)
			mask &= ~(1 << i);
	}

	return mask;
}

void Blackhole (netadr_t *from, qboolean isAutomatic, int mask, int method, const char *fmt, ...)
{
	blackhole_t *temp;
	va_list		argptr;

	if (isAutomatic && !sv_blackholes->intvalue)
		return;

	temp = &blackholes;

	while (temp->next)
		temp = temp->next;

	temp->next = Z_TagMalloc(sizeof(blackhole_t), TAGMALLOC_BLACKHOLE);
	temp = temp->next;

	temp->next = NULL;

	temp->ip = *(uint32 *)from->ip;
	temp->mask = CalcMask(mask);
	temp->method = method;

	temp->ratelimit.period = 1000;

	va_start (argptr,fmt);
	vsnprintf (temp->reason, sizeof(temp->reason)-1, fmt, argptr);
	va_end (argptr);

	//terminate
	temp->reason[sizeof(temp->reason)-1] = 0;

	if (sv.state)
		Com_Printf ("Added %s/%d to blackholes for %s.\n", LOG_SERVER|LOG_EXPLOIT, NET_inet_ntoa (temp->ip), mask, temp->reason);
}

qboolean UnBlackhole (int index)
{
	blackhole_t *temp, *last;
	int i;

	i = index;

	last = temp = &blackholes;
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

	Z_Free (temp);

	return true;
}

static const char *FindPlayer (netadr_t *from)
{
	int			i;
	client_t	*cl;

	for (i=0, cl=svs.clients ; i<maxclients->intvalue ; i++,cl++)
	{
		//FIXME: do we want packets from zombies still?
		if (cl->state == cs_free)
			continue;

		if (!NET_CompareBaseAdr (from, &cl->netchan.remote_address))
			continue;

		return cl->name;
	}

	return "";
}

/*
===============
SVC_RemoteCommand

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
static void SVC_RemoteCommand (void)
{
	//static int last_rcon_time = 0;
	int		i;
	char	remaining[2048];

	//only bad passwords get rate limited
	if (RateLimited (&svs.ratelimit_badrcon, 1))
	{
		Com_DPrintf ("SVC_RemoteCommand: Dropped rcon request from %s\n", NET_AdrToString (&net_from));
		return;
	}

	i = Rcon_Validate ();

	if (!i)
		RateSample (&svs.ratelimit_badrcon);

	Com_BeginRedirect (RD_PACKET, sv_outputbuf, sv_rcon_buffsize->intvalue, SV_FlushRedirect);

	if (!i)
	{
		Com_Printf ("Bad rcon_password.\n", LOG_GENERAL);
		Com_EndRedirect (true);
		Com_Printf ("Bad rcon from %s[%s] (%s).\n", LOG_SERVER|LOG_ERROR, NET_AdrToString (&net_from), FindPlayer(&net_from), Cmd_Args());
	}
	else
	{
		qboolean	endRedir;

		remaining[0] = 0;
		endRedir = true;

		//hack to allow clients to send rcon set commands properly
		if (!Q_stricmp (Cmd_Argv(2), "set"))
		{
			char		setvar[2048];
			qboolean	serverinfo = false;
			int			params;
			int			j;

			setvar[0] = 0;
			params = Cmd_Argc();

			if (!Q_stricmp (Cmd_Argv(params-1), "s"))
			{
				serverinfo = true;
				params--;
			}

			for (j=4 ; j<params ; j++)
			{
				strcat (setvar, Cmd_Argv(j) );
				if (j+1 != params)
					strcat (setvar, " ");
			}

			Com_sprintf (remaining, sizeof(remaining), "set %s \"%s\"%s", Cmd_Argv(3), setvar, serverinfo ? " s" : "");
		}
		//FIXME: This is a nice idea, but currently redirected output blocks until full buffer occurs, making it useless.
		/*else if (!Q_stricmp (Cmd_Argv(2), "monitor"))
		{
			Sys_ConsoleOutput ("Console monitor request from ");
			Sys_ConsoleOutput (NET_AdrToString (&net_from));
			Sys_ConsoleOutput ("\n");
			if (!Q_stricmp (Cmd_Argv(3), "on"))
			{
				Com_Printf ("Console monitor enabled.\n", LOG_SERVER);
				return;
			}
			else
			{
				Com_Printf ("Console monitor disabled.\n", LOG_SERVER);
				Com_EndRedirect (true);
				return;
			}
		}*/
		else
		{
			Q_strncpy (remaining, Cmd_Args2(2), sizeof(remaining)-1);
		}

		if (strlen(remaining) == sizeof(remaining)-1)
		{
			Com_Printf ("Rcon string length exceeded, discarded.\n", LOG_SERVER|LOG_WARNING);
			remaining[0] = 0;
		}

		//r1: limited rcon support
		if (i == 2)
		{
			linkednamelist_t	*lrcon;
			size_t				clen;

			i = 0;

			//len = strlen(remaining);

			if (strchr (remaining, ';'))
			{
				Com_Printf ("You may not use multiple commands in a single rcon command.\n", LOG_SERVER);
			}
			else
			{			
				for (lrcon = lrconcmds.next; lrcon; lrcon = lrcon->next)
				{
					clen = strlen (lrcon->name);

					if (!strncmp (remaining, lrcon->name, clen))
					{
						i = 1;
						break;
					}
				}

				if (!i)
					Com_Printf ("Rcon command '%s' is not allowed.\n", LOG_SERVER, remaining);
			}
		}

		if (i)
		{
			Cmd_ExecuteString (remaining);
			Com_EndRedirect (true);
			Com_Printf ("Rcon from %s[%s]:\n%s\n", LOG_SERVER, NET_AdrToString (&net_from), FindPlayer(&net_from), remaining);
		}
		else
		{
			Com_EndRedirect (true);
			Com_Printf ("Bad limited rcon from %s[%s]:\n%s\n", LOG_SERVER, NET_AdrToString (&net_from), FindPlayer(&net_from), remaining);
		}
	}

	//check for remote kill
	if (!svs.initialized)
		Com_Error (ERR_HARD, "server killed via rcon");
}

#ifdef USE_PYROADMIN
static void SVC_PyroAdminCommand (char *s)
{
	if (pyroadminid && atoi(Cmd_Argv(1)) == pyroadminid)
	{
		char	*p = s;
		char	remaining[1024];

		memset (remaining, 0, sizeof(remaining));

		while (*p && *p != ' ')
			p++;

		p++;

		while (*p && *p != ' ')
			p++;

		p++;

		Com_Printf ("pyroadmin>> %s\n", LOG_SERVER, p);
		Cmd_ExecuteString (p);
	}
}
#endif

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
static void SV_ConnectionlessPacket (void)
{
	blackhole_t *blackhole = &blackholes;
	char	*s;
	char	*c;

	//r1: ignore packets if IP is blackholed for abuse
	while (blackhole->next)
	{
		blackhole = blackhole->next;
		if ((*(uint32 *)net_from.ip & blackhole->mask) == (blackhole->ip & blackhole->mask))
		{
			//do rate limiting in case there is some long-ish reason
			if (blackhole->method == BLACKHOLE_MESSAGE)
			{
				RateSample (&blackhole->ratelimit);
				if (!RateLimited (&blackhole->ratelimit, 2))
					Netchan_OutOfBandPrint (NS_SERVER, &net_from, "print\n%s\n", blackhole->reason);
			}
			return;
		}
	}

	//r1: should never happen, don't even bother trying to parse it
	if (net_message.cursize > 544)
	{
		Com_DPrintf ("dropped %d byte connectionless packet from %s\n", net_message.cursize, NET_AdrToString (&net_from));
		return;
	}

	//r1: make sure we never talk to ourselves
	if (NET_IsLocalAddress (&net_from) && !NET_IsLocalHost(&net_from) && ShortSwap(net_from.port) == server_port)
	{
		Com_DPrintf ("dropped %d byte connectionless packet from self! (spoofing attack?)\n", net_message.cursize);
		return;
	}

	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message);		// skip the -1 marker

	s = MSG_ReadStringLine (&net_message);

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv(0);
	Com_DPrintf ("Packet %s : %s\n", NET_AdrToString(&net_from), c);

	if (!strcmp(c, "ping"))
		SVC_Ping ();
	else if (!strcmp(c,"status"))
		SVC_Status ();
	else if (!strcmp(c,"info"))
		SVC_Info ();
	else if (!strcmp(c,"getchallenge"))
		SVC_GetChallenge ();
	else if (!strcmp(c,"connect"))
		SVC_DirectConnect ();
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand ();
	else if (!strcmp(c, "ack"))
		SVC_Ack ();
#ifdef USE_PYROADMIN
	else if (!strcmp (c, "cmd"))
		SVC_PyroAdminCommand (s);
#endif
	else
		Com_DPrintf ("bad connectionless packet from %s:\n%s\n"
		, NET_AdrToString (&net_from), s);
}


//============================================================================

/*
===================
SV_CalcPings

Updates the cl->ping variables
===================
*/
static void SV_CalcPings (void)
{
	int			i, j;
	client_t	*cl;
	int			total, count;
	int			best;

	for (i=0 ; i<maxclients->intvalue ; i++)
	{
		cl = &svs.clients[i];
		if (cl->state != cs_spawned )
			continue;

		if (sv_calcpings_method->intvalue == 1)
		{
			total = 0;
			count = 0;
			for (j=0 ; j<LATENCY_COUNTS ; j++)
			{
				if (cl->frame_latency[j] > 0)
				{
					count++;
					total += cl->frame_latency[j];
				}
			}
			if (!count)
				cl->ping = 0;
			else
				cl->ping = total / count;
		}
		else if (sv_calcpings_method->intvalue == 2)
		{
			best = 9999;
			for (j=0 ; j<LATENCY_COUNTS ; j++)
			{
				if (cl->frame_latency[j] > 0 && cl->frame_latency[j] < best)
				{
					best = cl->frame_latency[j];
				}
			}

			cl->ping = best != 9999 ? best : 0;
		}
		else
		{
			cl->ping = 0;
		}

		// let the game dll know about the ping
#ifdef ENHANCED_SERVER
		((struct gclient_new_s *)(cl->edict->client))->ping = cl->ping;
#else
		((struct gclient_old_s *)(cl->edict->client))->ping = cl->ping;
#endif
	}
}


/*
===================
SV_GiveMsec

Every few frames, gives all clients an allotment of milliseconds
for their command moves.  If they exceed it, assume cheating.
===================
*/
static void SV_GiveMsec (void)
{
	int			msecpoint;
	int			i;
	client_t	*cl;

	if (sv.framenum & 15)
		return;

	//for checking against whether client is lagged out (no packets) or cheating (high msec)
	msecpoint = svs.realtime - 2500;

	for (i=0 ; i<maxclients->intvalue ; i++)
	{
		cl = &svs.clients[i];

		if (cl->state <= cs_zombie )
			continue;

		//hasn't sent a movement packet, could be irc bot for example
		if (!cl->moved)
			continue;

		if (cl->state == cs_spawned)
		{
			//r1: better? speed cheat detection via use of msec underflows
			if (cl->commandMsec < 0)
			{
#ifndef _DEBUG
				//don't spam listen servers in release
				if (!(Com_ServerState() && !dedicated->intvalue))
#endif
					Com_DPrintf ("SV_GiveMsec: %s has commandMsec < 0: %d (lagging/speed cheat?)\n", cl->name, cl->commandMsec);
				cl->commandMsecOverflowCount += 1.0f + (-cl->commandMsec / 250.0f);
			}
			else if (cl->commandMsec > sv_msecs->intvalue * 0.5)
			{
				//they aren't lagged out (still sending packets) but didn't even use half their msec. wtf?
				if (cl->lastmessage > msecpoint)
				{
#ifndef _DEBUG
				//don't spam listen servers in release
				if (!(Com_ServerState() && !dedicated->intvalue))
#endif
					Com_DPrintf ("SV_GiveMsec: %s has commandMsec %d (lagging/float cheat?)\n", cl->name, cl->commandMsec);
					cl->commandMsecOverflowCount += 1.0f * (cl->commandMsec / (float)sv_msecs->intvalue);
				}
			}
			else
			{
				//normal movement, drop counter a bit
				cl->commandMsecOverflowCount *= 0.985f;
			}

			if (cl->commandMsecOverflowCount > 1.0f)
				Com_DPrintf ("%s has %.2f overflowCount\n", cl->name, cl->commandMsecOverflowCount);

			if (sv_enforcetime->intvalue > 1 && cl->commandMsecOverflowCount >= sv_enforcetime->value)
			{
				SV_KickClient (cl, "irregular movement", "You were kicked from the game for irregular movement. This could be caused by excessive lag or other network conditions.\n");
				continue;
			}
		}

		cl->commandMsec = sv_msecs->intvalue;		// 1600 + some slop
	}
}


/*
=================
SV_ReadPackets
=================
*/
static void SV_ReadPackets (void)
{
	int			i, j;
	client_t	*cl;
	uint16		qport;

	//Com_Printf ("ReadPackets\n");
	for (;;)
	{
		j = NET_GetPacket (NS_SERVER, &net_from, &net_message);
		if (!j)
			break;

		if (j == -1)
		{
			// check for packets from connected clients
			for (i=0, cl=svs.clients ; i<maxclients->intvalue ; i++,cl++)
			{
				if (cl->state <= cs_zombie)
					continue;

				if (!NET_CompareAdr (&net_from, &cl->netchan.remote_address))
					continue;

				// r1: only drop if we haven't seen a packet for a bit
				if (cl->lastmessage > svs.realtime - 1500)
					continue;

				SV_KickClient (cl, "connection reset by peer", NULL);
				break;
			}
			continue;
		}

		// check for connectionless packet (0xffffffff) first
		if (*(int *)net_message_buffer == -1)
		{
			SV_ConnectionlessPacket ();
			continue;
		}

		// read the qport out of the message so we can fix up
		// stupid address translating routers
		//MSG_BeginReading (&net_message);
		//MSG_ReadLong (&net_message);		// sequence number
		//MSG_ReadLong (&net_message);		// sequence number
		
		//MSG_ReadShort (&net_message);

		qport = *(uint16 *)(net_message_buffer + 8);

		// check for packets from connected clients
		for (i=0, cl=svs.clients ; i<maxclients->intvalue ; i++,cl++)
		{
			//FIXME: do we want packets from zombies still?
			if (cl->state == cs_free)
				continue;

			if (!NET_CompareBaseAdr (&net_from, &cl->netchan.remote_address))
				continue;

			//qport shit
			if (cl->protocol == ORIGINAL_PROTOCOL_VERSION)
			{
				//compare short from original q2
				if (cl->netchan.qport != qport)
					continue;
			}
			else
			{
				//compare byte in newer r1q2, older r1q2 clients get qport zeroed on svc_directconnect
				if (cl->netchan.qport && cl->netchan.qport != (qport & 0xFF))
					continue;
			}

			if (cl->netchan.remote_address.port != net_from.port)
			{
				Com_Printf ("SV_ReadPackets: fixing up a translated port for client %d (%s) [%d->%d]\n", LOG_SERVER|LOG_NOTICE, i, cl->name, (uint16)(ShortSwap(cl->netchan.remote_address.port)), (uint16)(ShortSwap(net_from.port)));
				cl->netchan.remote_address.port = net_from.port;
			}

			if (Netchan_Process(&cl->netchan, &net_message))
			{	// this is a valid, sequenced packet, so process it
				if (cl->state != cs_zombie)
				{
					cl->lastmessage = svs.realtime;	// don't timeout

					if (!(sv.demofile && sv.state == ss_demo))
						SV_ExecuteClientMessage (cl);
					cl->packetCount++;

					//r1: send a reply immediately if the client is connecting
					if ((cl->state == cs_connected || cl->state == cs_spawning) && cl->msgListStart->next)
					{
						SV_WriteReliableMessages (cl, cl->netchan.message.buffsize);
						Netchan_Transmit (&cl->netchan, 0, NULL);
					}
				}
			}
			break;
		}
		
		
		/* r1: a little pointless?
		if (i != maxclients->value)
			continue;
		*/
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client for timeout->value
seconds, drop the conneciton.  Server frames are used instead of
realtime to avoid dropping the local client while debugging.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
static void SV_CheckTimeouts (void)
{
	int		i;
	client_t	*cl;
	int			droppoint;
	int			zombiepoint;
	int			flagRun = 0;
	static int	lastRun = 0;

	droppoint = svs.realtime - 1000*timeout->intvalue;
	zombiepoint = svs.realtime - 1000*zombietime->intvalue;

	if (svs.realtime - lastRun > 5000)
	{
		lastRun = svs.realtime;
		flagRun = 1;
	}
	else if (lastRun > svs.realtime)
	{
		lastRun = svs.realtime;
	}

	for (i=0,cl=svs.clients ; i<maxclients->intvalue ; i++,cl++)
	{
		// message times may be wrong across a changelevel
		if (cl->lastmessage > svs.realtime)
			cl->lastmessage = svs.realtime;

		if (cl->state == cs_zombie && cl->lastmessage < zombiepoint)
		{
			SV_CleanClient (cl);
			cl->state = cs_free;	// can now be reused
			continue;
		}

		if (cl->state >= cs_connected)
		{
			if (flagRun)
			{
				//r1: ignore first few seconds after level change
				if (svs.realtime > 10000 && cl->state == cs_spawned)
				{
					cl->fps = (int)((float)cl->packetCount / 5.0);

					//r1ch: fps spam protection
					if (sv_fpsflood->intvalue && cl->fps > sv_fpsflood->intvalue)
						SV_KickClient (cl, va("too many packets/sec (%d)", cl->fps), "You were kicked from the game.\n(Tip: try lowering your cl_maxfps to avoid flooding the server)\n");
				}
				cl->packetCount = 0;
			}

			if (cl->state == cs_spawned)
			{
				cl->idletime++;

				//icky test
				if (sv_predict_on_lag->intvalue && (cl->lastmessage < svs.realtime - 200) && cl->moved)
				{
					int old = cl->lastcmd.msec;
					Com_DPrintf ("Lag predicting %s (lastMsg %d)\n", cl->name, svs.realtime - cl->lastmessage);
					cl->lastcmd.msec = 100;
					//SV_ClientThink (cl, cl->lastcmd);
					ge->ClientThink (cl->edict, &cl->lastcmd);
					cl->lastcmd.msec = old;
				}


				if (sv_idlekick->intvalue && cl->idletime >= sv_idlekick->intvalue * 10)
				{
					SV_KickClient (cl, "idling", "You have been disconnected due to inactivity.\n");
					continue;
				}
			}

			if (cl->lastmessage < droppoint)
			{
				//r1: only message if they spawned (less spam plz)
				if (cl->state == cs_spawned && cl->name[0])
					SV_BroadcastPrintf (PRINT_HIGH, "%s timed out\n", cl->name);
				Com_Printf ("Dropping %s, timed out.\n", LOG_SERVER|LOG_DROP, cl->name);
				SV_DropClient (cl, false); 
				SV_CleanClient (cl);
				cl->state = cs_free;	// don't bother with zombie state
			}
		}
	}
}

/*
================
SV_PrepWorldFrame

This has to be done before the world logic, because
player processing happens outside RunWorldFrame
================
*/
static void SV_PrepWorldFrame (void)
{
	edict_t	*ent;
	int		i;

	for (i=0 ; i<ge->num_edicts ; i++)
	{
		ent = EDICT_NUM(i);
		// events only last for a single message
		ent->s.event = 0;
	}
}

/*
=================
SV_RunGameFrame
=================
*/
static void SV_RunGameFrame (void)
{
#ifndef DEDICATED_ONLY
	if (host_speeds->intvalue)
		time_before_game = Sys_Milliseconds ();
#endif

	// we always need to bump framenum, even if we
	// don't run the world, otherwise the delta
	// compression can get confused when a client
	// has the "current" frame
	sv.framenum++;
	sv.time = sv.framenum*100;

	sv_tracecount = 0;

	// don't run if paused
	if (!sv_paused->intvalue || maxclients->intvalue > 1)
	{
		ge->RunFrame ();

		// never get more than one tic behind
		if (sv.time < svs.realtime)
		{
#ifndef DEDICATED_ONLY
			if (sv_showclamp->intvalue)
				Com_Printf ("sv highclamp\n", LOG_SERVER);
#endif
			svs.realtime = sv.time;
		}
	}

#ifndef DEDICATED_ONLY
	if (host_speeds->intvalue)
		time_after_game = Sys_Milliseconds ();
#endif

}

/*
================
Master_Heartbeat

Send a message to the master every few minutes to
let it know we are alive, and log information
================
*/

static void Master_Heartbeat (void)
{
	const char	*string;
	int			i;

	if (!dedicated->intvalue)
		return;		// only dedicated servers send heartbeats

	if (!public_server->intvalue)
		return;		// a private dedicated game

	// check for time wraparound
	if (svs.last_heartbeat > svs.realtime)
		svs.last_heartbeat = svs.realtime;

	if (svs.realtime - svs.last_heartbeat < HEARTBEAT_SECONDS*1000)
		return;		// not time to send yet

	svs.last_heartbeat = svs.realtime;

	// send the same string that we would give for a status OOB command
	string = SV_StatusString();

	// send to group master
	for (i=0 ; i<MAX_MASTERS ; i++)
	{
		if (master_adr[i].port)
		{
			Com_Printf ("Sending heartbeat to %s\n", LOG_SERVER|LOG_NOTICE, NET_AdrToString (&master_adr[i]));
			Netchan_OutOfBandPrint (NS_SERVER, &master_adr[i], "heartbeat\n%s", string);
		}
	}
}

/*
==================
SV_Frame

==================
*/
void SV_Frame (int msec)
{
#ifndef DEDICATED_ONLY
	time_before_game = time_after_game = 0;
#endif

	// if server is not active, do nothing
	if (!svs.initialized)
	{
		//DERP DERP only do this when not running a god damn client!!
		if (dedicated->intvalue)
			Sys_Sleep (1);
		return;
	}

    svs.realtime += msec;

	// keep the random time dependent
	//rand ();

	// get packets from clients
	SV_ReadPackets ();

	// move autonomous things around if enough time has passed
	if (!sv_timedemo->intvalue && svs.realtime < sv.time)
	{
		// never let the time get too far off
		if (sv.time - svs.realtime > 100)
		{
#ifndef DEDICATED_ONLY
			if (sv_showclamp->intvalue)
				Com_Printf ("sv lowclamp\n", LOG_SERVER);
#endif
			svs.realtime = sv.time - 100;
		}

		//r1: execute commands now
		if (dedicated->intvalue)
		{
			Cbuf_Execute();
			if (!svs.initialized)
				return;
		}
		NET_Sleep(sv.time - svs.realtime);
		return;
	}

	//r1: execute commands now
	Cbuf_Execute();

	//may have executed some kind of quit
	if (!svs.initialized)
		return;

	// update ping based on the last known frame from all clients
	SV_CalcPings ();

	// give the clients some timeslices
	SV_GiveMsec ();

	// let everything in the world think and move
	SV_RunGameFrame ();

	// check timeouts
	SV_CheckTimeouts ();

	// send messages back to the clients that had packets read this frame
	SV_SendClientMessages ();

	// save the entire world state if recording a serverdemo
	SV_RecordDemoMessage ();

	// send a heartbeat to the master if needed
	Master_Heartbeat ();

	// clear teleport flags, etc for next frame
	SV_PrepWorldFrame ();
}

//============================================================================

/*
=================
Master_Shutdown

Informs all masters that this server is going down
=================
*/
static void Master_Shutdown (void)
{
	int			i;

	//r1: server died before cvars had a chance to init!
	if (!dedicated)
		return;

	if (!dedicated->intvalue)
		return;		// only dedicated servers send heartbeats

	if (public_server && !public_server->intvalue)
		return;		// a private dedicated game

	// send to group master
	for (i=0 ; i<MAX_MASTERS ; i++)
		if (master_adr[i].port)
		{
			if (i > 0)
				Com_Printf ("Sending shutdown to %s\n", LOG_SERVER|LOG_NOTICE, NET_AdrToString (&master_adr[i]));
			Netchan_OutOfBandPrint (NS_SERVER, &master_adr[i], "shutdown");
		}
}

//============================================================================




void UserinfoBanDrop (const char *key, const banmatch_t *ban, const char *result)
{
	if (ban->message[0])
		SV_ClientPrintf (sv_client, PRINT_HIGH, "%s\n", ban->message);

	if (ban->blockmethod == CVARBAN_BLACKHOLE)
		Blackhole (&sv_client->netchan.remote_address, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "userinfovarban: %s == %s", key, result);

	Com_Printf ("Dropped client %s, userinfoban: %s == %s\n", LOG_SERVER|LOG_DROP, sv_client->name, key, result);

	SV_DropClient (sv_client, (ban->blockmethod != CVARBAN_BLACKHOLE));
}

qboolean SV_UserInfoBanned (client_t *cl)
{
	char				key[MAX_INFO_KEY];
	const banmatch_t	*match;

	key[0] = 0;
	while ((match = SV_CheckUserinfoBans (cl->userinfo, key)))
	{
		switch (match->blockmethod)
		{
			case CVARBAN_MESSAGE:
				SV_ClientPrintf (cl, PRINT_HIGH, "%s\n", match->message);
				break;
			case CVARBAN_STUFF:
				MSG_BeginWriting (svc_stufftext);
				MSG_WriteString (va ("%s\n",match->message));
				SV_AddMessage (cl, true);
				break;
			case CVARBAN_LOGONLY:
				Com_Printf ("LOG: %s[%s] matched userinfoban: %s == %s\n", LOG_SERVER, cl->name, NET_AdrToString (&cl->netchan.remote_address), key, Info_ValueForKey (cl->userinfo, key));
				break;
			case CVARBAN_EXEC:
				Cbuf_AddText (match->message);
				Cbuf_AddText ("\n");
				break;
			default:
				UserinfoBanDrop (key, match, Info_ValueForKey (cl->userinfo, key));
				return true;
		}
	}

	return false;
}

/*
=================
SV_UserinfoChanged

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_UserinfoChanged (client_t *cl)
{
	char				*val;

	if (cl->state < cs_connected)
		Com_Printf ("Warning, SV_UserinfoChanged for unconnected client %s[%s]!!\n", LOG_SERVER|LOG_WARNING, cl->name, NET_AdrToString (&cl->netchan.remote_address));

	if (!cl->userinfo[0])
		Com_Printf ("Warning, SV_UserinfoChanged for %s[%s] with empty userinfo!\n", LOG_SERVER|LOG_WARNING, cl->name, NET_AdrToString (&cl->netchan.remote_address));

	//r1ch: ban anyone trying to use the end-of-message-in-string exploit
	if (strchr (cl->userinfo, '\xFF'))
	{
		char		*ptr;
		const char	*p;
		ptr = strchr (cl->userinfo, '\xFF');
		ptr -= 8;
		if (ptr < cl->userinfo)
			ptr = cl->userinfo;
		p = MakePrintable (ptr, 0);
		Com_Printf ("EXPLOIT: Client %s[%s] supplied userinfo string containing 0xFF: %s\n", LOG_EXPLOIT|LOG_SERVER, cl->name, NET_AdrToString (&cl->netchan.remote_address), p);
		Blackhole (&cl->netchan.remote_address, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "0xFF in userinfo (%.32s)", p);
		SV_KickClient (cl, "illegal userinfo", NULL);
		return;
	}

	if (!SV_UserinfoValidate (cl->userinfo))
	{
		//Com_Printf ("WARNING: SV_UserinfoValidate failed for %s[%s] (%s)\n", LOG_SERVER|LOG_WARNING, cl->name, NET_AdrToString (&cl->netchan.remote_address), MakePrintable (cl->userinfo));
		Com_Printf ("EXPLOIT: Client %s[%s] supplied an illegal userinfo string: %s\n", LOG_EXPLOIT|LOG_SERVER, cl->name, NET_AdrToString (&cl->netchan.remote_address), MakePrintable (cl->userinfo, 0));
		Blackhole (&cl->netchan.remote_address, true, sv_blackhole_mask->intvalue, BLACKHOLE_SILENT, "illegal userinfo string");
		SV_KickClient (cl, "illegal userinfo", NULL);
		return;
	}

	if (sv_strict_userinfo_check->intvalue)
	{
		if (!Info_CheckBytes (cl->userinfo))
		{
			Com_Printf ("Warning, illegal userinfo bytes from %s.\n", LOG_SERVER|LOG_WARNING, cl->name);
			SV_KickClient (cl, "illegal userinfo string", "Userinfo contains illegal bytes. Please disable any color-names and similar features.\n");
			return;
		}
	}

	//r1ch: allow filtering of stupid "fun" names etc.
	if (sv_filter_userinfo->intvalue)
		StripHighBits(cl->userinfo, (int)sv_filter_userinfo->intvalue == 2);

	val = Info_ValueForKey (cl->userinfo, "name");

	//they tried to set name to something bad
	if (!val[0] || NameColorFilterCheck (val) || StringIsWhitespace(val))
	{
		Com_Printf ("Warning, invalid name change to '%s' from %s[%s].\n", LOG_SERVER|LOG_WARNING, MakePrintable(val, 0), cl->name, NET_AdrToString(&cl->netchan.remote_address)); 
		SV_ClientPrintf (cl, PRINT_HIGH, "Invalid name '%s'\n", val);
		if (cl->name[0])
		{
			//force them back to old name
			MSG_BeginWriting (svc_stufftext);
			MSG_WriteString (va("set name \"%s\"\n", cl->name));
			SV_AddMessage (cl, true);
			Info_SetValueForKey (cl->userinfo, "name", cl->name);
		}
		else
		{
			//no old name, bye
			Com_DPrintf ("dropping %s for malformed userinfo (%s)\n", cl->name, cl->userinfo);
			SV_KickClient (cl, "bad userinfo", NULL);
			return;
		}
	}

	if (Info_KeyExists (cl->userinfo, "ip"))
	{
		Com_Printf ("Dropping %s for attempted IP spoof (%s)\n", LOG_SERVER|LOG_DROP, cl->name, cl->userinfo);
		SV_KickClient (cl, "bad userinfo", NULL);
		return;
	}

	if (SV_UserInfoBanned (cl))
		return;

	//this actually fills in the client fields
	SV_UpdateUserinfo (cl, true);
}

static void SV_UpdateWindowTitle (cvar_t *cvar, char *old, char *newvalue)
{
	if (dedicated->intvalue)
	{
		char buff[512];
		Com_sprintf (buff, sizeof(buff), "%s - R1Q2 " VERSION " (port %d)", newvalue, server_port);

		//for win32 this will set window titlebar text
		//for linux this currently does nothing
		Sys_SetWindowText (buff);
	}
}

static void _password_changed (cvar_t *var, char *oldvalue, char *newvalue)
{
	if (!newvalue[0] || !strcmp (newvalue, "none"))
		Cvar_FullSet ("needpass", "0", CVAR_SERVERINFO);
	else
		Cvar_FullSet ("needpass", "1", CVAR_SERVERINFO);
}

static void _rcon_buffsize_changed (cvar_t *var, char *oldvalue, char *newvalue)
{
	if (var->intvalue > SV_OUTPUTBUF_LENGTH)
	{
		Cvar_SetValue (var->name, SV_OUTPUTBUF_LENGTH);
	}
	else if (var->intvalue < 256)
	{
		Cvar_Set (var->name, "256");
	}
}

/*
===============
SV_Init

Only called at quake2.exe startup, not for each game
===============
*/

void SV_Init (void)
{
	SV_InitOperatorCommands	();

	rcon_password = Cvar_Get ("rcon_password", "", 0);
	lrcon_password = Cvar_Get ("lrcon_password", "", 0);

	Cvar_Get ("skill", "1", 0);

	//r1: default 1
	Cvar_Get ("deathmatch", "1", CVAR_LATCH);
	Cvar_Get ("coop", "0", CVAR_LATCH);
	Cvar_Get ("dmflags", va("%i", DF_INSTANT_ITEMS), CVAR_SERVERINFO);
	Cvar_Get ("fraglimit", "0", CVAR_SERVERINFO);
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);
	Cvar_Get ("cheats", "0", CVAR_SERVERINFO|CVAR_LATCH);
	Cvar_Get ("protocol", va("%i", ENHANCED_PROTOCOL_VERSION), CVAR_NOSET);

	//r1: default 8
	maxclients = Cvar_Get ("maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);

	hostname = Cvar_Get ("hostname", "Unnamed R1Q2 Server", CVAR_SERVERINFO | CVAR_ARCHIVE);

	//r1: update window title on the fly
	hostname->changed = SV_UpdateWindowTitle;
	hostname->changed (hostname, hostname->string, hostname->string);

	//r1: dropped to 90 from 125
	timeout = Cvar_Get ("timeout", "90", 0);

	//r1: bumped to 3 from 2
	zombietime = Cvar_Get ("zombietime", "3", 0);

#ifndef DEDICATED_ONLY
	sv_showclamp = Cvar_Get ("showclamp", "0", 0);
#endif

	sv_paused = Cvar_Get ("paused", "0", 0);
	sv_timedemo = Cvar_Get ("timedemo", "0", 0);

	//r1: default 1
	sv_enforcetime = Cvar_Get ("sv_enforcetime", "1", 0);

#ifndef NO_SERVER
	allow_download = Cvar_Get ("allow_download", "0", CVAR_ARCHIVE);
	allow_download_players  = Cvar_Get ("allow_download_players", "1", CVAR_ARCHIVE);
	allow_download_models = Cvar_Get ("allow_download_models", "1", CVAR_ARCHIVE);
	allow_download_sounds = Cvar_Get ("allow_download_sounds", "1", CVAR_ARCHIVE);
	allow_download_maps	  = Cvar_Get ("allow_download_maps", "1", CVAR_ARCHIVE);
	allow_download_pics	  = Cvar_Get ("allow_download_pics", "1", CVAR_ARCHIVE);
	allow_download_textures = Cvar_Get ("allow_download_textures", "1", 0);
	allow_download_others = Cvar_Get ("allow_download_others", "0", 0);
#endif

	sv_noreload = Cvar_Get ("sv_noreload", "0", 0);

	sv_airaccelerate = Cvar_Get("sv_airaccelerate", "0", CVAR_LATCH);

	public_server = Cvar_Get ("public", "0", 0);

	//r1: not needed
	//sv_reconnect_limit = Cvar_Get ("sv_reconnect_limit", "3", CVAR_ARCHIVE);

	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	//r1: init pyroadmin support
#ifdef USE_PYROADMIN
	pyroadminport = Cvar_Get ("pyroadminport", "0", CVAR_NOSET);
#endif

	//r1: lock server (prevent new connections)
	sv_locked = Cvar_Get ("sv_locked", "0", 0);

	//r1: restart server on this map if the _GAME_ dll dies
	//    (only via a ERR_GAME drop of course)
	sv_restartmap = Cvar_Get ("sv_restartmap", "", 0);

	//r1: server-side password protection (why id put this into the game dll
	//    i will never figure out)
	sv_password = Cvar_Get ("sv_password", "", 0);
	sv_password->changed = _password_changed;

	//r1: text filtering: 1 = strip low bits, 2 = low+hi
	sv_filter_q3names = Cvar_Get ("sv_filter_q3names", "0", 0);
	sv_filter_userinfo = Cvar_Get ("sv_filter_userinfo", "0", 0);
	sv_filter_stringcmds = Cvar_Get ("sv_filter_stringcmds", "0", 0);

	//r1: enable blocking of clients that attempt to attack server/clients
	sv_blackholes = Cvar_Get ("sv_blackholes", "1", 0);

	//r1: allow clients to use cl_nodelta (increases server bw usage)
	sv_allownodelta = Cvar_Get ("sv_allownodelta", "1", 0);

	//r1: option to block q2ace due to hacked versions
	sv_deny_q2ace = Cvar_Get ("sv_deny_q2ace", "0", 0);
	if (sv_deny_q2ace->intvalue)
		Com_Printf ("WARNING: sv_deny_q2ace is deprecated and will be removed in a future build.\n", LOG_SERVER|LOG_WARNING);

	//r1: limit connections per ip address (stop zombie dos/flood)
	sv_iplimit = Cvar_Get ("sv_iplimit", "3", 0);

	//r1: message to send to connecting clients via CONNECTIONLESS print immediately
	//    after client connects. \n is expanded to new line.
	sv_connectmessage = Cvar_Get ("sv_connectmessage", "", 0);

	//r1: nocheat visibility check support (cpu intensive -- warning)
	sv_nc_visibilitycheck = Cvar_Get ("sv_nc_visibilitycheck", "0", 0);
	sv_nc_clientsonly = Cvar_Get ("sv_nc_clientsonly", "1", 0);

	//r1: http dl server
	sv_downloadserver = Cvar_Get ("sv_downloadserver", "", 0);

	//r1: max allowed file size for autodownloading (bytes)
	sv_max_download_size = Cvar_Get ("sv_max_download_size", "8388608", 0);

	//r1: max backup packets to allow from lagged clients (id.default=20)
	sv_max_netdrop = Cvar_Get ("sv_max_netdrop", "20", 0);

	//r1: don't respond to status requests
	sv_hidestatus = Cvar_Get ("sv_hidestatus", "0", 0);

	//r1: hide player info from status requests
	sv_hideplayers = Cvar_Get ("sv_hideplayers", "0", 0);

	//r1: kick high fps users flooding packets
	sv_fpsflood = Cvar_Get ("sv_fpsflood", "0", 0);

	//r1: randomize starting framenum to thwart map timers
	sv_randomframe = Cvar_Get ("sv_randomframe", "0", 0);

	//r1: msecs to give clients
	sv_msecs = Cvar_Get ("sv_msecs", "1800", 0);

	//r1: nocheat parser shit
	sv_nc_kick = Cvar_Get ("sv_nc_kick", "0", 0);
	if (sv_nc_kick->intvalue)
		Com_Printf ("WARNING: sv_nc_kick is deprecated and will be removed in a future build.\n", LOG_SERVER|LOG_WARNING);

	sv_nc_announce = Cvar_Get ("sv_nc_announce", "0", 0);
	if (sv_nc_announce->intvalue)
		Com_Printf ("WARNING: sv_nc_announce is deprecated and will be removed in a future build.\n", LOG_SERVER|LOG_WARNING);

	sv_filter_nocheat_spam = Cvar_Get ("sv_filter_nocheat_spam", "0", 0);
	if (sv_filter_nocheat_spam->intvalue)
		Com_Printf ("WARNING: sv_filter_nocheat_spam is deprecated and will be removed in a future build.\n", LOG_SERVER|LOG_WARNING);

	//r1: crash on game errors with int3
	sv_gamedebug = Cvar_Get ("sv_gamedebug", "0", 0);

	//r1: reload game dll on next map change (resets to 0 after)
	sv_recycle = Cvar_Get ("sv_recycle", "0", 0);

	//r1: track server uptime in serverinfo?
	sv_uptime = Cvar_Get ("sv_uptime", "0", 0);

	//r1: allow strafe jumping at high fps values (Requires hacked client (!!!))
	sv_strafejump_hack = Cvar_Get ("sv_strafejump_hack", "0", CVAR_LATCH);

	//r1: reserved slots (set 'rp' userinfo)
	sv_reserved_password = Cvar_Get ("sv_reserved_password", "", 0);
	sv_reserved_slots = Cvar_Get ("sv_reserved_slots", "0", CVAR_LATCH);

	//r1: allow use of 'map' command after game dll is loaded?
	sv_allow_map = Cvar_Get ("sv_allow_map", "0", 0);

	//r1: allow unconnected clients to execute game commands (default enabled in id!)
	sv_allow_unconnected_cmds = Cvar_Get ("sv_allow_unconnected_cmds", "0", 0);

	//r1: validate userinfo strings explicitly according to info key rules?
	sv_strict_userinfo_check = Cvar_Get ("sv_strict_userinfo_check", "1", 0);

	//r1: method to calculate pings. 0 = disable entirely, 1 = average, 2 = min
	sv_calcpings_method = Cvar_Get ("sv_calcpings_method", "1", 0);

	//r1: disallow mod to set serverinfo via Cvar_Get?
	sv_no_game_serverinfo = Cvar_Get ("sv_no_game_serverinfo", "0", 0);

	//r1: send message on download failure/success
	sv_mapdownload_denied_message = Cvar_Get ("sv_mapdownload_denied_message", "", 0);
	sv_mapdownload_ok_message = Cvar_Get ("sv_mapdownload_ok_message", "", 0);

	//r1: failsafe against buggy traces
	sv_max_traces_per_frame = Cvar_Get ("sv_max_traces_per_frame", "10000", 0);

	//r1: rate limiting for status requests to prevent udp spoof DoS
	sv_ratelimit_status = Cvar_Get ("sv_ratelimit_status", "15", 0);

	//r1: allow new SVF_ ent flags? some mods mistakenly extend SVF_ for their own purposes.
	sv_new_entflags = Cvar_Get ("sv_new_entflags", "0", 0);

	//r1: validate playerskins passed to us by the mod? if not, we could end up sending
	//illegal names (eg "name\hack/$$") which the client would issue as "download hack/$$"
	//resulting in a command expansion attack.
	sv_validate_playerskins = Cvar_Get ("sv_validate_playerskins", "1", 0);

	//r1: seconds before idle kick
	sv_idlekick = Cvar_Get ("sv_idlekick", "0", 0);

	//r1: cut off packetentities if they get too large?
	sv_packetentities_hack = Cvar_Get ("sv_packetentities_hack", "0", 0);

	//r1: don't send ents that are marked !inuse?
	sv_entity_inuse_hack = Cvar_Get ("sv_entity_inuse_hack", "0", 0);

	//r1: force reconnect?
	sv_force_reconnect = Cvar_Get ("sv_force_reconnect", "", 0);

	//r1: refuse downloads if this many players connected
	sv_download_refuselimit = Cvar_Get ("sv_download_refuselimit", "0", 0);

	sv_download_drop_file = Cvar_Get ("sv_download_drop_file", "", 0);
	sv_download_drop_message = Cvar_Get ("sv_download_drop_message", "This mod requires client-side files, please visit the mod homepage to download them.", 0);

	//r1: default blackhole mask
	sv_blackhole_mask = Cvar_Get ("sv_blackhole_mask", "32", 0);

	//r1: what to do on unrequested cvars
	sv_badcvarcheck = Cvar_Get ("sv_badcvarcheck", "1", 0);

	//r1: control rcon buffer size
	sv_rcon_buffsize = Cvar_Get ("sv_rcon_buffsize", "1384", 0);
	sv_rcon_buffsize->changed = _rcon_buffsize_changed;

	//r1: show output of rcon in console?
	sv_rcon_showoutput = Cvar_Get ("sv_rcon_showoutput", "0", 0);

	//r1: broadcast name changes?
	sv_show_name_changes = Cvar_Get ("sv_show_name_changes", "0", 0);

	//r1: delta optimz (small non-r1q2 client breakage on 2)
	sv_optimize_deltas = Cvar_Get ("sv_optimize_deltas", "1", 0);

	//r1: q3-style delta entities?
	sv_advanced_deltas = Cvar_Get ("sv_advanced_deltas", "0", CVAR_LATCH);

	//r1: enhanced setplayer code?
	sv_enhanced_setplayer = Cvar_Get ("sv_enhanced_setplayer", "0", 0);

	//r1: test lag stuff
	sv_predict_on_lag = Cvar_Get ("sv_predict_on_lag", "0", 0);

#ifdef ANTICHEAT
	sv_require_anticheat = Cvar_Get ("sv_require_anticheat", "0", CVAR_LATCH);
	sv_anticheat_message = Cvar_Get ("sv_anticheat_message", "This server requires the r1ch.net anticheat module. Please see http://www.r1ch.net/stuff/r1q2/anticheat/ for more details.", 0);
#endif

	//r1: init pyroadmin
#ifdef USE_PYROADMIN
	if (pyroadminport->intvalue)
	{
		char buff[128];
		int len;

		NET_StringToAdr ("127.0.0.1", &netaddress_pyroadmin);
		netaddress_pyroadmin.port = ShortSwap((uint16)pyroadminport->intvalue);

		pyroadminid = Sys_Milliseconds() & 0xFFFF;

		len = Com_sprintf (buff, sizeof(buff), "hello %d", pyroadminid);
		Netchan_OutOfBand (NS_SERVER, &netaddress_pyroadmin, len, (byte *)buff);
	}
#endif
}

/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
static void SV_FinalMessage (const char *message, qboolean reconnect)
{
	int			i;
	client_t	*cl;
	
	SZ_Clear (&net_message);
	MSG_Clear();
	MSG_BeginWriting (svc_print);
	MSG_WriteByte (PRINT_HIGH);
	MSG_WriteString (message);
	MSG_EndWriting (&net_message);

	if (reconnect)
		SZ_WriteByte (&net_message, svc_reconnect);
	else
		SZ_WriteByte (&net_message, svc_disconnect);

	// send it twice
	// stagger the packets to crutch operating system limited buffers

	for (i=0, cl = svs.clients ; i<maxclients->intvalue ; i++, cl++)
		if (cl->state >= cs_connected)
		{
			Netchan_Transmit (&cl->netchan, net_message.cursize
			, net_message_buffer);
		}

	for (i=0, cl = svs.clients ; i<maxclients->intvalue ; i++, cl++)
		if (cl->state >= cs_connected)
		{
			Netchan_Transmit (&cl->netchan, net_message.cursize
			, net_message_buffer);
		}
}



/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown (char *finalmsg, qboolean reconnect, qboolean crashing)
{
	if (svs.clients)
		SV_FinalMessage (finalmsg, reconnect);

	Master_Shutdown ();

	if (!crashing || dbg_unload->intvalue)
		SV_ShutdownGameProgs ();

	// free current level
	if (sv.demofile)
		fclose (sv.demofile);
	memset (&sv, 0, sizeof(sv));
	Com_SetServerState (sv.state);

	// free server static data
	if (svs.clients)
		Z_Free (svs.clients);

	if (svs.client_entities)
		Z_Free (svs.client_entities);

	if (svs.demofile)
		fclose (svs.demofile);

	Cvar_ForceSet ("$mapname", "");
	Cvar_ForceSet ("$game", "");

	memset (&svs, 0, sizeof(svs));

	MSG_Clear();
}
