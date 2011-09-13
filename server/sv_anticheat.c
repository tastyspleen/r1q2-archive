/*
Copyright (C) 2006 r1ch.net

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

//r1ch.net anticheat server interface for Quake II

#ifdef ANTICHEAT
#include "server.h"

#ifndef _WIN32
	#include <unistd.h>
	#include <sys/socket.h>
	#include <sys/types.h>
 	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/time.h>
	#include <netdb.h>
	#include <sys/param.h>
	#include <sys/ioctl.h>
	#include <sys/uio.h>
	#include <errno.h>
	#define closesocket close
	#define ioctlsocket ioctl
	#define SOCKET unsigned int
	#define SOCKET_ERROR -1
#else
	#define WIN32_LEAN_AND_MEAN
	#include <winsock.h>
#endif

void SV_AntiCheat_Disconnect(void);
qboolean SV_AntiCheat_Connect (void);
static qboolean SV_AntiCheat_ReadFile (const char *filename, void (*func)(char *, int, const char *));
qboolean	NET_StringToSockaddr (const char *s, struct sockaddr *sadr);

#define DEFAULT_BACKOFF 5

#define	AC_BUFFSIZE	131072

int		packetLen;
byte	packetBuffer[1024];
SOCKET acSocket;
struct sockaddr_in	acServer;
int		retryBackOff = DEFAULT_BACKOFF;
time_t	retryTime;
int		expectedLength;
qboolean	connect_pending;
qboolean	anticheat_ready;

size_t	acSendBufferLen;
byte	acSendBuffer[AC_BUFFSIZE];

int		antiCheatNumFileHashes;

static unsigned	last_ping = 0;
static qboolean ping_pending = false;

typedef struct filehash_s
{
	struct filehash_s	*next;
	byte				hash[20];
	char				quakePath[MAX_QPATH];
	int					flags;
} filehash_t;

filehash_t	fileHashes;

char anticheat_hashlist_name[256];

typedef enum
{
	OP_INVALID,
	OP_EQUAL,
	OP_NEQUAL,
	OP_GTEQUAL,
	OP_LTEQUAL,
	OP_LT,
	OP_GT,
	OP_STREQUAL,
	OP_STRNEQUAL,
	OP_STRSTR,
} cvarop_e;

typedef struct cvarcheck_s
{
	struct cvarcheck_s	*next;
	char				*var_name;
	cvarop_e			op;
	char				**var_values;
	unsigned int		num_values;
	char				*default_value;
} cvarcheck_t;

cvarcheck_t	cvarChecks;

static int antiCheatNumCvarChecks;

linkednamelist_t	anticheat_tokens;

enum acserverbytes_e
{
	ACS_BAD,
	ACS_CLIENTACK,
	ACS_VIOLATION,
	ACS_NOACCESS,
	ACS_FILE_VIOLATION,
	ACS_READY,
	ACS_QUERYREPLY,
	ACS_PONG,
	ACS_UPDATE_REQUIRED,
	ACS_DISCONNECT,
	ACS_ERROR,
};

enum q2serverbytes_e
{
	Q2S_BAD,
	Q2S_VERSION,
	Q2S_PREF,
	Q2S_REQUESTCHALLENGE,
	Q2S_CLIENTDISCONNECT,
	Q2S_QUERYCLIENT,
	Q2S_PING,
	Q2S_UPDATECHECKS,
	Q2S_SETPREFERENCES,
};

const char *anticheat_client_names[] = 
{
	"???",
	"R1Q2",
	"EGL",
	"Apr GL",
	"Apr SW",
	"Q2PRO",
};

#define ACP_BLOCKPLAY	(1<<0)

//note, max of 8 unless SV_AntiCheat_SendFileAndCvarChecks is changed also
#define	ACH_REQUIRED	(1<<0)
#define	ACH_NEGATIVE	(1<<1)

#define ANTICHEAT_PROTOCOL_VERSION	0xAC03

static void SV_AntiCheat_ClearFileHashes (void)
{
	filehash_t	*f, *last = NULL;

	f = &fileHashes;

	while (f->next)
	{
		f = f->next;
		if (last)
			Z_Free (last);
		last = f;
	}

	if (last)
		Z_Free (last);

	fileHashes.next = NULL;
	antiCheatNumFileHashes = 0;
}

static void SV_AntiCheat_ClearCvarChecks (void)
{
	int			i;
	cvarcheck_t *f, *last = NULL;

	f = &cvarChecks;

	while (f->next)
	{
		f = f->next;
		if (last)
		{
			Z_Free (last->var_name);
			for (i = 0; i < last->num_values; i++)
				Z_Free (last->var_values[i]);
			Z_Free (last->var_values);
			Z_Free (last);
		}
		last = f;
	}

	if (last)
	{
		for (i = 0; i < last->num_values; i++)
			Z_Free (last->var_values[i]);
		Z_Free (last->var_name);
		Z_Free (last->var_values);
		Z_Free (last);
	}

	cvarChecks.next = NULL;
	antiCheatNumCvarChecks = 0;
}

static void SV_AntiCheat_ClearTokens (void)
{
	linkednamelist_t	*t, *last = NULL;

	t = &anticheat_tokens;

	while (t->next)
	{
		t = t->next;
		if (last)
		{
			Z_Free (last->name);
			Z_Free (last);
		}
		last = t;
	}

	if (last)
	{
		Z_Free (last->name);
		Z_Free (last);
	}

	anticheat_tokens.next = NULL;
}

int	HexToRaw (const char *c)
{
	int temp;

	if (isdigit(c[0]))
		temp = c[0] - '0';
	else if (isxdigit(c[0]))
		temp = tolower(c[0]) - 'a' + 10;
	else
		return -1;

	temp *= 16;

	c++;

	if (isdigit(c[0]))
		temp += c[0] - '0';
	else if (isxdigit(c[0]))
		temp += tolower(c[0]) - 'a' + 10;
	else
		return -1;

	return temp;
}

static void SV_AntiCheat_ParseCvarLine (char *line, int line_number, const char *filename)
{
	cvarcheck_t *checks;
	char		*p, *q;
	char		*var_name, *op, *var_value, *default_value;
	cvarop_e	eop;
	int			num_values, i;
	char		**tokens;

	p = strchr (line, '\t');
	if (!p)
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line);
		return;
	}

	var_name = line;

	p[0] = 0;
	p++;

	op = p;

	if (!p[0])
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line);
		return;
	}

	p = strchr (op, '\t');
	if (!p)
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line);
		return;
	}

	p[0] = 0;
	p++;

	var_value = p;

	if (!p[0])
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line);
		return;
	}

	p = strchr (var_value, '\t');
	if (!p)
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line);
		return;
	}

	p[0] = 0;
	p++;

	default_value = p;

	if (strlen (var_name) >= 64 || !var_name[0])
	{
		Com_Printf ("ANTICHEAT WARNING: Invalid cvar name '%s' in anticheat-cvars.txt at line %d\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, var_name, line_number);
		return;
	}

	if (strlen (default_value) >= 64 || !default_value[0])
	{
		Com_Printf ("ANTICHEAT WARNING: Invalid default value '%s' in anticheat-cvars.txt at line %d\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, default_value, line_number);
		return;
	}

	num_values = 1;

	p = var_value;
	while (p)
	{
		p = strchr (p, ',');
		if (p)
		{
			num_values++;
			p++;
		}
	}

	if (num_values >= 255)
	{
		Com_Printf ("ANTICHEAT WARNING: Too many values on line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line);
		return;
	}

	tokens = Z_TagMalloc (num_values * sizeof(char *), TAGMALLOC_ANTICHEAT);

	i = 0;
	p = q = var_value;
	while (p)
	{
		p = strchr (p, ',');
		if (p)
		{
			p[0] = 0;
			tokens[i++] = q;
			p++;
			q = p;
		}
		else
			tokens[i++] = q;
	}

	for (i = 0; i < num_values; i++)
	{
		if (strlen (tokens[i]) > 64 || !tokens[i][0])
		{
			Com_Printf ("ANTICHEAT WARNING: Bad value '%s' on line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, tokens[i], line_number, line);
			Z_Free (tokens);
			return;
		}
	}

	if (!strcmp (op, "=") || !strcmp (op, "=="))
	{
		eop = OP_EQUAL;
	}
	else if (!strcmp (op, "!="))
	{
		eop = OP_NEQUAL;
	}
	else if (!strcmp (op, ">="))
	{
		if (num_values > 1)
		{
			Z_Free (tokens);
			Com_Printf ("ANTICHEAT WARNING: Unsupported multiple values with op '%s' on line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, op, line_number, line);
			return;
		}
		eop = OP_GTEQUAL;
	}
	else if (!strcmp (op, "<="))
	{
		if (num_values > 1)
		{
			Z_Free (tokens);
			Com_Printf ("ANTICHEAT WARNING: Unsupported multiple values with op '%s' on line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, op, line_number, line);
			return;
		}
		eop = OP_LTEQUAL;
	}
	else if (!strcmp (op, ">"))
	{
		if (num_values > 1)
		{
			Z_Free (tokens);
			Com_Printf ("ANTICHEAT WARNING: Unsupported multiple values with op '%s' on line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, op, line_number, line);
			return;
		}
		eop = OP_GT;
	}
	else if (!strcmp (op, "<"))
	{
		if (num_values > 1)
		{
			Z_Free (tokens);
			Com_Printf ("ANTICHEAT WARNING: Unsupported multiple values with op '%s' on line %d '%s' in anticheat-cvars.txt\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, op, line_number, line);
			return;
		}
		eop = OP_LT;
	}
	else if (!strcmp (op, "eq"))
	{
		eop = OP_STREQUAL;
	}
	else if (!strcmp (op, "ne"))
	{
		eop = OP_STRNEQUAL;
	}
	else if (!strcmp (op, "~"))
	{
		eop = OP_STRSTR;
	}
	else
	{
		Z_Free (tokens);
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in anticheat-cvars.txt: unknown op '%s'\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line, op);
		return;
	}

	checks = &cvarChecks;

	while (checks->next)
		checks = checks->next;

	checks->next = Z_TagMalloc (sizeof (*checks), TAGMALLOC_ANTICHEAT);
	checks = checks->next;

	checks->next = NULL;
	checks->var_name = CopyString (var_name, TAGMALLOC_ANTICHEAT);
	checks->op = eop;

	checks->var_values = Z_TagMalloc (num_values * sizeof(char *), TAGMALLOC_ANTICHEAT);
	for (i = 0; i < num_values; i++)
		checks->var_values[i] = CopyString (tokens[i], TAGMALLOC_ANTICHEAT);
	
	checks->num_values = num_values;

	checks->default_value = CopyString (default_value, TAGMALLOC_ANTICHEAT);

	Z_Free (tokens);
	
	antiCheatNumCvarChecks++;
}

static void SV_AntiCheat_ParseHashLine (char *line, int line_number, const char *filename)
{
	filehash_t	*hashes;
	int			i;
	int			flags;
	char		*p, *hash;

	if (line[0] == '!')
	{
		strncpy (anticheat_hashlist_name, line + 1, sizeof(anticheat_hashlist_name)-1);
		ExpandNewLines (anticheat_hashlist_name);
		return;
	}

	p = strchr (line, '\t');
	if (!p)
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed line %d '%s' in %s\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line_number, line, filename);
		return;
	}

	p[0] = 0;
	p++;

	hash = p;

	p = strchr (hash, '\t');
	if (p)
	{
		p[0] = 0;
		p++;
	}

	if (strlen (hash) != 40)
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed hash '%s' in %s on line %d\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, hash, filename, line_number);
		return;
	}

	for (i = 0; i < 40; i++)
	{
		if (!isxdigit (hash[i]))
		{
			Com_Printf ("ANTICHEAT WARNING: Malformed hash '%s' in %s on line %d\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, hash, filename, line_number);
			return;
		}
	}

	if (strlen (line) >= MAX_QPATH || strchr (line, '\\') || !isalnum(line[0]))
	{
		Com_Printf ("ANTICHEAT WARNING: Malformed quake path '%s' in %s on line %d\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line, filename, line_number);
		return;
	}

	Q_strlwr (line);

	flags = 0;

	if (p && p[0])
	{
		if (strstr (p, "required"))
			flags |= ACH_REQUIRED;
		if (strstr (p, "negative"))
			flags |= ACH_NEGATIVE;
	}

	hashes = &fileHashes;

	while (hashes->next)
		hashes = hashes->next;

	hashes->next = Z_TagMalloc (sizeof(*hashes), TAGMALLOC_ANTICHEAT);
	hashes = hashes->next;
	hashes->next = NULL;

	for (i = 0; i < 20; i++)
		hashes->hash[i] = HexToRaw (hash + i*2);
	
	hashes->flags = flags;

	strcpy (hashes->quakePath, line);
	antiCheatNumFileHashes++;
}

static void SV_AntiCheat_ParseToken (char *line, int line_number, const char *filename)
{
	linkednamelist_t	*t;

	t = &anticheat_tokens;

	while (t->next)
		t = t->next;

	t->next = Z_TagMalloc (sizeof(*t), TAGMALLOC_ANTICHEAT);
	t = t->next;

	t->next = NULL;
	t->name = CopyString (line, TAGMALLOC_ANTICHEAT);
}

static qboolean SV_AntiCheat_ReadFile (const char *filename, void (*func)(char *, int, const char *))
{
	int			len;
	char		line[256];
	char		*q;
	char		*buff, *ptr;
	int			line_number;

	len = FS_LoadFile (filename, (void **)&buff);

	if (len == -1)
		return false;

	ptr = buff;
	q = buff;

	line_number = 1;

	while (len)
	{
		switch (buff[0])
		{
			case '\n':
			case '\r':
				buff[0] = 0;
				if (q)
				{
					qboolean	parse;
					char		*p;

					Q_strncpy (line, q, sizeof(line)-1);
					parse = true;

					p = strchr (line, '\n');
					if (p)
						p[0] = 0;

					p = strchr (line, '\r');
					if (p)
						p[0] = 0;

					if (line[0] == '#' || line[0] == '/' || line[0] == '\0')
						parse = false;
					else if (line[0] == '\\')
					{
						if (!strncmp (line + 1, "include ", 8))
						{
							char	*path;

							path = line + 9;

							if (!SV_AntiCheat_ReadFile (path, func))
								Com_Printf ("ANTICHEAT WARNING: Unable to read included file '%s' from line %d of %s\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, path, line_number, filename);
						}
						else
						{
							Com_Printf ("ANTICHEAT WARNING: Unknown directive '%s' on line %d of %s\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER, line + 1, line_number, filename);
						}

						parse = false;
					}

					if (parse)
						func (line, line_number, filename);
					q = NULL;
					line_number++;
				}
				buff++;
				break;
			case '\0':
				buff++;
				break;
			default:
				if (!q)
					q = buff;
				buff++;
				break;
		}
		len--;
	}

	FS_FreeFile (ptr);

	return true;
}

static void SV_AntiCheat_Unexpected_Disconnect (void)
{
	int		i;

	if (!acSocket)
		return;

	closesocket (acSocket);
	acSocket = 0;

	if (anticheat_ready)
		retryBackOff = DEFAULT_BACKOFF;
	else
		retryBackOff += 30;	//this generally indicates a server problem

	retryTime = time(NULL) + retryBackOff;

	//reset everyone to failure status
	for (i = 0; i < maxclients->intvalue; i++)
	{
		svs.clients[i].anticheat_valid = false;
		svs.clients[i].anticheat_file_failures = 0;
	}

	last_ping = 0;
	ping_pending = false;

	//inform
	if (anticheat_ready)
	{
		SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " This server has lost the connection to the anticheat server. Any anticheat clients are no longer valid.\n");

		if (sv_require_anticheat->intvalue == 2)
			SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " You will need to reconnect once the server has re-established the anticheat connection.\n");
	}

	Com_Printf ("ANTICHEAT WARNING: Lost connection to anticheat server! Will attempt to reconnect in %d seconds.\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT, retryBackOff);

	anticheat_ready = false;
}

//this is different from the violation "disconnected" as this message is only sent if
//the client manually disconnected and exists to prevent the race condition of the server
//seeing the disconnect violation before the udp message and thus showing "%s lost connection"
//right before the player leaves the server
static void SV_AntiCheat_ParseDisconnect (byte *buff, int bufflen)
{
	client_t		*cl;
	unsigned short	clientID;
	uint32			challenge;

	if (bufflen < 3)
		return;

	clientID = *(unsigned short *)buff;
	buff += 2;
	bufflen -= 2;

	//we check challenge to ensure we don't get a race condition if a client reconnects.
	challenge = *(uint32 *)buff;
	buff += 4;
	bufflen -= 4;

	if (clientID >= maxclients->intvalue)
	{
		Com_Printf ("ANTICHEAT WARNING: ParseDisconnect with illegal client ID %d\n", LOG_ANTICHEAT|LOG_WARNING, clientID);
		return;
	}

	cl = &svs.clients[clientID];

	if (cl->challenge != challenge)
		return;

	if (cl->state >= cs_connected)
	{
		Com_Printf ("Dropping %s, anticheat disconnect message.\n", LOG_SERVER, cl->name);
		SV_DropClient (cl, false);
	}
}

static void SV_AntiCheat_ParseViolation (byte *buff, int bufflen)
{
	int				len;
	client_t		*cl;
	unsigned short	clientID;
	const char		*reason, *clientreason;
	uint32			challenge;

	if (bufflen < 7)
		return;

	clientID = *(unsigned short *)buff;
	buff += 2;
	bufflen -= 2;

	//we check challenge to ensure we don't get a race condition if a client reconnects.
	challenge = *(uint32 *)buff;
	buff += 4;
	bufflen -= 4;

	reason = (const char *)buff;

	len = (int)strlen(reason) + 1;

	buff += len;
	bufflen -= len;

	if (bufflen)
		clientreason = (const char *)buff;
	else
		clientreason = NULL;

	if (clientID >= maxclients->intvalue)
	{
		Com_Printf ("ANTICHEAT WARNING: ParseViolation with illegal client ID %d\n", LOG_ANTICHEAT|LOG_WARNING, clientID);
		return;
	}

	cl = &svs.clients[clientID];

	if (cl->challenge != challenge)
		return;

	if (cl->state >= cs_connected)
	{
		//FIXME: should we notify other players about anticheat violations found before clientbegin?
		//one side says yes to expose cheaters, other side says no since client will have no previous
		//message to show that they're trying to join. currently showing messages only for spawned clients.

		//fixme maybe
		if (strcmp (reason, "disconnected"))
		{
			char	showreason[32];

			if (sv_anticheat_show_violation_reason->intvalue)
				Com_sprintf (showreason, sizeof(showreason), " (%s)", reason);
			else
				showreason[0] = 0;

			if (cl->state == cs_spawned)
				SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " %s was kicked for anticheat violation%s\n", cl->name, showreason);
			else
				SV_ClientPrintf (cl, PRINT_HIGH, ANTICHEATMESSAGE " %s was kicked for anticheat violation%s\n", cl->name, showreason);

			Com_Printf ("ANTICHEAT VIOLATION: %s[%s] was kicked: '%s'\n", LOG_SERVER|LOG_ANTICHEAT, cl->name, NET_AdrToString (&cl->netchan.remote_address), reason);

			if (clientreason)
				SV_ClientPrintf (cl, PRINT_HIGH, "%s\n", clientreason);

			//hack to fix late zombies race condition
			cl->lastmessage = svs.realtime;

			SV_DropClient (cl, true);
		}
		else
		{
			int		method;

			if (!cl->anticheat_valid)
				return;

			Com_Printf ("ANTICHEAT DISCONNECT: %s[%s] disconnected from anticheat server\n", LOG_SERVER|LOG_ANTICHEAT, cl->name, NET_AdrToString (&cl->netchan.remote_address));

			if (cl->anticheat_required == ANTICHEAT_REQUIRED)
				method = sv_anticheat_forced_disconnect_action->intvalue;
			else
				method = sv_anticheat_client_disconnect_action->intvalue;

			if (method == 1)
			{
				if (cl->state == cs_spawned)
					SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " %s lost connection to anticheat server.\n", cl->name);
				else
					SV_ClientPrintf (cl, PRINT_HIGH, ANTICHEATMESSAGE " Your connection to the anticheat server was lost.\n");
				SV_DropClient (cl, true);
				return;
			}
			else
			{
				if (cl->state == cs_spawned)
					SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " %s lost connection to anticheat server, client is no longer valid.\n", cl->name);
				else
					SV_ClientPrintf (cl, PRINT_HIGH, ANTICHEATMESSAGE " Your connection to the anticheat server was lost.\n");
				cl->anticheat_valid = false;
			}
		}
	}
	//else if (cl->state != cs_zombie)
	//	Com_Printf ("ANTICHEAT WARNING: Violation on %s[%s] in state %d: '%s'\n", LOG_SERVER|LOG_WARNING|LOG_ANTICHEAT, cl->name, NET_AdrToString (&cl->netchan.remote_address), cl->state, reason);
}

static void SV_AntiCheat_ParseFileViolation (byte *buff, int bufflen)
{
	linkednamelist_t	*bad;
	client_t			*cl;
	unsigned short		clientID;
	const char			*quakePath, *failedhash;
	int					len;
	uint32				challenge;

	if (bufflen < 7)
		return;

	clientID = *(unsigned short *)buff;
	buff += 2;
	bufflen -= 2;

	//we check challenge to ensure we don't get a race condition if a client reconnects.
	challenge = *(uint32 *)buff;
	buff += 4;
	bufflen -= 4;

	quakePath = (const char *)buff;

	len = (int)strlen(quakePath) + 1;

	buff += len;
	bufflen -= len;

	if (bufflen)
		failedhash = (const char *)buff;
	else
		failedhash = "no hash?";

	if (clientID >= maxclients->intvalue)
	{
		Com_Printf ("ANTICHEAT WARNING: ParseFileViolation with illegal client ID %d\n", LOG_ANTICHEAT|LOG_WARNING, clientID);
		return;
	}

	cl = &svs.clients[clientID];

	if (cl->challenge != challenge)
		return;

	if (cl->state >= cs_connected)
	{
		int			action;
		filehash_t	*f;

		cl->anticheat_file_failures++;

		action = sv_anticheat_badfile_action->intvalue;

		f = &fileHashes;
		while (f->next)
		{
			f = f->next;
			if (!strcmp (f->quakePath, quakePath))
			{
				if (f->flags & ACH_REQUIRED)
				{
					action = 0;
					break;
				}
			}
		}

		Com_Printf ("ANTICHEAT FILE VIOLATION: %s[%s] has a modified %s [%s]\n", LOG_SERVER|LOG_ANTICHEAT, cl->name, NET_AdrToString (&cl->netchan.remote_address), quakePath, failedhash);
		switch (action)
		{
			case 0:
				if (cl->state == cs_spawned)
					SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " %s was kicked for modified %s\n", cl->name, quakePath);
				else
					SV_ClientPrintf (cl, PRINT_HIGH, ANTICHEATMESSAGE " %s was kicked for modified %s\n", cl->name, quakePath);

				//show custom msg
				if (sv_anticheat_badfile_message->string[0])
					SV_ClientPrintf (cl, PRINT_HIGH, "%s\n", sv_anticheat_badfile_message->string);

				//hack to fix late zombies race condition
				cl->lastmessage = svs.realtime;
				
				SV_DropClient (cl, true);
				return;
			case 1:
				SV_ClientPrintf (cl, PRINT_HIGH, "WARNING: Your file %s has been modified. Please replace it with a known valid copy.\n", quakePath);

				//show custom msg
				if (sv_anticheat_badfile_message->string[0])
					SV_ClientPrintf (cl, PRINT_HIGH, "%s\n", sv_anticheat_badfile_message->string);
				break;
			case 2:
				//spamalicious :)
				if (cl->state == cs_spawned)
					SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " %s has a modified %s\n", cl->name, quakePath);
				else
					SV_ClientPrintf (cl, PRINT_HIGH, ANTICHEATMESSAGE " %s has a modified %s\n", cl->name, quakePath);

				//show custom msg
				if (sv_anticheat_badfile_message->string[0])
					SV_ClientPrintf (cl, PRINT_HIGH, "%s\n", sv_anticheat_badfile_message->string);
				break;
		}

		if (cl->state != cs_zombie && sv_anticheat_badfile_max->intvalue &&
			cl->anticheat_file_failures >= sv_anticheat_badfile_max->intvalue)
		{
			SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " %s was kicked for too many modified files\n", cl->name);

			//broadcasts are dropped until in-game, repeat if necessary so the client has a clue wtf is going on
			if (cl->state != cs_spawned)
				SV_ClientPrintf (cl, PRINT_HIGH, ANTICHEATMESSAGE " %s was kicked for too many modified files\n", cl->name);

			//hack to fix late zombies race condition
			cl->lastmessage = svs.realtime;

			SV_DropClient (cl, true);
			return;
		}

		bad = &cl->anticheat_bad_files;
		while (bad->next)
			bad = bad->next;

		bad->next = Z_TagMalloc (sizeof(*bad), TAGMALLOC_ANTICHEAT);
		bad = bad->next;
		bad->name = CopyString (quakePath, TAGMALLOC_ANTICHEAT);
		bad->next = NULL;
	}
	//else if (cl->state > cs_zombie)
	//	Com_Printf ("ANTICHEAT WARNING: File violation on %s[%s] in state %d: '%s'\n", LOG_SERVER|LOG_WARNING|LOG_ANTICHEAT, cl->name, NET_AdrToString (&cl->netchan.remote_address), cl->state, quakePath);
}

static void SV_AntiCheat_ParseClientAck (byte *buff, int bufflen)
{
	client_t		*cl;
	unsigned short	clientID;
	uint32			challenge;
	int				client_type;

	if (bufflen < 7)
		return;

	clientID = *(unsigned short *)buff;
	buff += 2;
	bufflen -= 2;

	//we check challenge to ensure we don't get a race condition if a client reconnects.
	challenge = *(uint32 *)buff;
	buff += 4;
	bufflen -= 4;

	client_type = (int)(*buff);

	if (clientID >= maxclients->intvalue)
	{
		Com_Printf ("ANTICHEAT WARNING: ParseClientAck with illegal client ID %d\n", LOG_ANTICHEAT|LOG_WARNING, clientID);
		return;
	}

	cl = &svs.clients[clientID];

	if (cl->challenge != challenge)
		return;

	if (cl->state != cs_connected && cl->state != cs_spawning)
	{
		Com_DPrintf ("ANTICHEAT WARNING: ParseClientAck with client in state %d\n",cl->state);
		return;
	}

	cl->anticheat_client_type = client_type;
	cl->anticheat_valid = true;
}

static void SV_AntiCheat_ParseReady (void)
{
	//SV_BroadcastPrintf (PRINT_HIGH, ANTICHEATMESSAGE " Anticheat server connection established. Please reconnect if you are using an anticheat-capable client.\n");
	anticheat_ready = true;
	retryBackOff = DEFAULT_BACKOFF;
	Com_Printf ("ANTICHEAT: Ready to serve anticheat clients.\n", LOG_ANTICHEAT);

	Cvar_FullSet ("anticheat", sv_require_anticheat->string, CVAR_SERVERINFO | CVAR_NOSET);
}

static void SV_AntiCheat_ParseQueryReply (byte *buff, int bufflen)
{
	client_t		*cl;
	unsigned short	clientID;
	uint32			challenge;
	int				client_type, valid;

	if (bufflen < 7)
		return;

	clientID = *(unsigned short *)buff;
	bufflen -= 2;
	buff += 2;

	//we check challenge to ensure we don't get a race condition if a client reconnects.
	challenge = *(uint32 *)buff;
	buff += 4;
	bufflen -= 4;

	valid = (int) (*buff);
	buff++;
	bufflen--;

	client_type = (int) (*buff);

	if (clientID >= maxclients->intvalue)
	{
		Com_Printf ("ANTICHEAT WARNING: ParseQueryReply with illegal client ID %d\n", LOG_ANTICHEAT|LOG_WARNING, clientID);
		return;
	}

	cl = &svs.clients[clientID];

	if (cl->challenge != challenge)
		return;

	if (cl->state != cs_spawning)
	{
		Com_DPrintf ("ANTICHEAT WARNING: ParseQueryReply with client in state %d\n", cl->state);
		if (cl->state > cs_zombie)
			SV_DropClient (cl, true);
		return;
	}

	cl->anticheat_query_sent = ANTICHEAT_QUERY_DONE;

	if (valid == 1)
	{
		cl->anticheat_client_type = client_type;
		cl->anticheat_valid = true;
	}

	SV_ClientBegin (cl);
}

static void SV_AntiCheat_ParseBuffer (void)
{
	byte	 *buff;
	int		bufflen;

	if (!packetLen)
		return;

	buff = packetBuffer;
	bufflen = packetLen;

	Com_DPrintf ("Anticheat packet type %d\n", buff[0]);

	switch (buff[0])
	{
		case ACS_VIOLATION:
			SV_AntiCheat_ParseViolation (buff + 1, bufflen - 1);
			break;
		case ACS_CLIENTACK:
			SV_AntiCheat_ParseClientAck (buff + 1, bufflen - 1);
			break;
		case ACS_FILE_VIOLATION:
			SV_AntiCheat_ParseFileViolation (buff + 1, bufflen - 1);
			break;
		case ACS_READY:
			SV_AntiCheat_ParseReady ();
			break;
		case ACS_QUERYREPLY:
			SV_AntiCheat_ParseQueryReply (buff + 1, bufflen - 1);
			break;
		case ACS_ERROR:
			Com_Printf ("ANTICHEAT ERROR: %s\n", LOG_ANTICHEAT|LOG_ERROR|LOG_SERVER, buff + 1);
			SV_AntiCheat_Disconnect ();
			Cvar_ForceSet ("sv_anticheat_required", "0");
			break;
		case ACS_NOACCESS:
			Com_Printf ("ANTICHEAT WARNING: You do not have permission to use the anticheat server. Anticheat disabled.\n", LOG_ANTICHEAT|LOG_WARNING|LOG_SERVER);
			SV_AntiCheat_Disconnect ();
			Cvar_ForceSet ("sv_anticheat_required", "0");
			break;
		case ACS_UPDATE_REQUIRED:
			Com_Printf ("ANTICHEAT WARNING: The anticheat server is no longer compatible with this version of R1Q2. Please make sure you are using the latest R1Q2 server version. Anticheat disabled.\n", LOG_ANTICHEAT|LOG_WARNING|LOG_SERVER);
			SV_AntiCheat_Disconnect ();
			Cvar_ForceSet ("sv_anticheat_required", "0");
			break;
		case ACS_DISCONNECT:
			SV_AntiCheat_ParseDisconnect (buff + 1, bufflen - 1);
			break;
		case ACS_PONG:
			ping_pending = false;
			break;
		default:
			Com_Printf ("ANTICHEAT ERROR: Unknown command byte %d, please make sure you are using the latest R1Q2 server version. Anticheat disabled.\n", LOG_ANTICHEAT|LOG_WARNING|LOG_SERVER, buff[0]);
			SV_AntiCheat_Disconnect ();
			Cvar_ForceSet ("sv_anticheat_required", "0");
			break;
	}
}

static qboolean SV_AntiCheat_Spin (void)
{
	while (acSendBufferLen >= AC_BUFFSIZE / 2)
	{
		//flush as much as we can
		SV_AntiCheat_Run ();
		if (!acSocket)
			return false;
		Sys_Sleep (1);
	}
	return true;
}

static int SV_AntiCheat_SendPreferences (void)
{
	uint32	preferences;

	preferences = 0;

	if (sv_anticheat_disable_play->intvalue)
		preferences |= ACP_BLOCKPLAY;

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 5;
	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_SETPREFERENCES;

	memcpy (acSendBuffer + acSendBufferLen, &preferences, sizeof(preferences));
	acSendBufferLen += sizeof(preferences);

	return 1;
}

void SV_AntiCheat_UpdatePrefs (cvar_t *v, char *oldVal, char *newVal)
{
	SV_AntiCheat_SendPreferences ();
}

static int SV_AntiCheat_SendFileAndCvarChecks (void)
{
	filehash_t		*f;
	cvarcheck_t		*c;
	const char		*lastPath;

	SV_AntiCheat_ClearFileHashes ();

	if (!SV_AntiCheat_ReadFile ("anticheat-hashes.txt", SV_AntiCheat_ParseHashLine))
	{
		Com_Printf ("ANTICHEAT WARNING: Missing anticheat-hashes.txt, not using any file checks.\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
	}
	else if (!fileHashes.next)
	{
		Com_Printf ("ANTICHEAT WARNING: No file hashes were loaded, please check the anticheat-hashes.txt.\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER);
	}

	if (antiCheatNumFileHashes && !anticheat_hashlist_name[0])
		Com_sprintf (anticheat_hashlist_name, sizeof(anticheat_hashlist_name), "unknown (%d %s)", antiCheatNumFileHashes, antiCheatNumFileHashes == 1 ? "entry" : "entries");

	SV_AntiCheat_ClearCvarChecks ();

	if (!SV_AntiCheat_ReadFile ("anticheat-cvars.txt", SV_AntiCheat_ParseCvarLine))
	{
		Com_Printf ("ANTICHEAT WARNING: Missing anticheat-cvars.txt, not using any cvar checks.\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
	}
	else if (!cvarChecks.next)
	{
		Com_Printf ("ANTICHEAT WARNING: No cvar checks were loaded, please check the anticheat-cvars.txt.\n", LOG_WARNING|LOG_ANTICHEAT|LOG_SERVER);
	}

	SV_AntiCheat_ClearTokens ();
	SV_AntiCheat_ReadFile ("anticheat-tokens.txt", SV_AntiCheat_ParseToken);

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 9;
	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_UPDATECHECKS;

	memcpy (acSendBuffer + acSendBufferLen, &antiCheatNumFileHashes, sizeof(antiCheatNumFileHashes));
	acSendBufferLen += sizeof(antiCheatNumFileHashes);

	memcpy (acSendBuffer + acSendBufferLen, &antiCheatNumCvarChecks, sizeof(antiCheatNumCvarChecks));
	acSendBufferLen += sizeof(antiCheatNumCvarChecks);

	//this gets really nasty now, since file hashes are plentiful they will go way above the 64k
	//packet limit and maybe even the whole sendbuffer size. so we can block here, hopefully we don't
	//get disconnected too often mid-game for this to be a problem.

	//flush as much as we can
	SV_AntiCheat_Run ();
	if (!acSocket)
		return 0;

	lastPath = NULL;
	f = &fileHashes;

	while (f->next)
	{
		f = f->next;

		//ick ick ick...
		if (acSendBufferLen + sizeof(*f) + 2 >= AC_BUFFSIZE)
		{
			Com_Printf ("ANTICHEAT WARNING: Send buffer length exceeded, server may be frozen for a short while!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
			if (!SV_AntiCheat_Spin ())
				return 0;
		}

		memcpy (acSendBuffer + acSendBufferLen, f->hash, sizeof(f->hash));
		acSendBufferLen += sizeof(f->hash);

		acSendBuffer[acSendBufferLen++] = (byte)f->flags;

		if (lastPath && !strcmp (f->quakePath, lastPath))
		{
			acSendBuffer[acSendBufferLen++] = 0;
		}
		else
		{
			size_t	length;
			length = strlen(f->quakePath);
			acSendBuffer[acSendBufferLen++] = (byte)length;
			memcpy (acSendBuffer + acSendBufferLen, f->quakePath, length);
			acSendBufferLen += length;
		}
		lastPath = f->quakePath;
	}

	c = &cvarChecks;
	while (c->next)
	{
		int		i;
		size_t	length;
		byte	b;

		c = c->next;
		
		length = 1 + 1 + 1 + strlen(c->var_name) + strlen (c->default_value);

		for (i = 0; i < c->num_values; i++)
			length += strlen (c->var_values[i]) + 1;

		//ick ick ick...
		if (acSendBufferLen + length >= AC_BUFFSIZE)
		{
			Com_Printf ("ANTICHEAT WARNING: Send buffer length exceeded, server may be frozen for a short while!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
			if (!SV_AntiCheat_Spin ())
				return 0;
		}

		b = (byte)strlen (c->var_name);
		acSendBuffer[acSendBufferLen++] = b;
		memcpy (acSendBuffer + acSendBufferLen, c->var_name, (size_t)b);
		acSendBufferLen += b;

		acSendBuffer[acSendBufferLen++] = (byte)c->op;
		acSendBuffer[acSendBufferLen++] = (byte)c->num_values;

		for (i = 0; i < c->num_values; i++)
		{
			b = (byte)strlen (c->var_values[i]);
			acSendBuffer[acSendBufferLen++] = b;
			memcpy (acSendBuffer + acSendBufferLen, c->var_values[i], (size_t)b);
			acSendBufferLen += b;
		}

		b = (byte)strlen (c->default_value);
		acSendBuffer[acSendBufferLen++] = b;
		memcpy (acSendBuffer + acSendBufferLen, c->default_value, (size_t)b);
		acSendBufferLen += b;
	}

	return 1;
}

static void SV_AntiCheat_Hello (void)
{
	unsigned short	len, hostlen, verlen;
	const char		*host;
	const char		*ver;
	size_t			index;

	acSendBufferLen = 1;
	acSendBuffer[0] = '\x02';

	host = hostname->string;
	ver = R1Q2_VERSION_STRING;

	hostlen = (unsigned short)strlen(host);
	verlen = (unsigned short)strlen(ver);

	len = 22 + hostlen + verlen;
	index = acSendBufferLen;

	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_VERSION;

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = ANTICHEAT_PROTOCOL_VERSION;
	acSendBufferLen += sizeof(unsigned short);

	memcpy (acSendBuffer + acSendBufferLen, &hostlen, sizeof(hostlen));
	acSendBufferLen += sizeof(hostlen);

	memcpy (acSendBuffer + acSendBufferLen, host, hostlen);
	acSendBufferLen += hostlen;

	memcpy (acSendBuffer + acSendBufferLen, &verlen, sizeof(verlen));
	acSendBufferLen += sizeof(verlen);

	memcpy (acSendBuffer + acSendBufferLen, ver, verlen);
	acSendBufferLen += verlen;

	memcpy (acSendBuffer + acSendBufferLen, &server_port, sizeof(server_port));
	acSendBufferLen += sizeof(server_port);

	*(unsigned short *)(acSendBuffer + index) = len;

	if (!SV_AntiCheat_SendFileAndCvarChecks ())
		return;

	if (!SV_AntiCheat_SendPreferences ())
		return;

	if (acSendBufferLen + 3 >= AC_BUFFSIZE)
	{
		Com_Printf ("ANTICHEAT WARNING: Send buffer length exceeded, server may be frozen for a short while!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
		if (!SV_AntiCheat_Spin())
			return;
	}

	//ping in case server is actually frozen. NOTE: if server is unable to upload all hashes within
	//15 seconds this will cause a problem. however a server with such slow upload should probably
	//not be up anyway :).
	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 1;
	acSendBufferLen += 2;
	acSendBuffer[acSendBufferLen++] = Q2S_PING;
	last_ping = Sys_Milliseconds ();
	ping_pending = true;
	//anticheat_ready = true;
}

static void SV_AntiCheat_Nag (client_t *cl)
{
	if (cl->anticheat_valid)
	{
		cl->anticheat_nag_time = 0;
		return;
	}

	if (cl->netchan.reliable_length == 0 && (strstr (cl->versionString, "Win32") || strstr (cl->versionString, "win32")))
	{
		MSG_BeginWriting (svc_stufftext);
		MSG_WriteString ("set _old_centertime $scr_centertime\n");
		SV_AddMessage (cl, true);

		MSG_BeginWriting (svc_stufftext);
		MSG_WriteString (va ("set scr_centertime %g\n", sv_anticheat_nag_time->value));
		SV_AddMessage (cl, true);

		//force a buffer flush so the stufftexts go through (yuck)
		SV_WriteReliableMessages (cl, cl->netchan.message.buffsize);
		Netchan_Transmit (&cl->netchan, 0, NULL);

		MSG_BeginWriting (svc_centerprint);
		MSG_WriteString (sv_anticheat_nag_message->string);
		SV_AddMessage (cl, true);

		MSG_BeginWriting (svc_stufftext);
		MSG_WriteString ("set scr_centertime $_old_centertime\n");
		SV_AddMessage (cl, true);

		cl->anticheat_nag_time = 0;
	}
}

static void SV_AntiCheat_CheckTimeOuts (void)
{
	client_t		*cl;

	if (ping_pending)
	{
		if ((unsigned)(curtime - last_ping) >= 15000)
		{
			ping_pending = false;
			Com_Printf ("ANTICHEAT: Anticheat server ping timeout, disconnecting.\n", LOG_ANTICHEAT|LOG_WARNING);
			SV_AntiCheat_Unexpected_Disconnect ();
			return;
		}
	}

	if (anticheat_ready)
	{
		//only ping if ready so we don't put data into the middle of a spin
		if ((unsigned)(curtime - last_ping) >= 60000)
		{
			last_ping = curtime;
			if (acSendBufferLen + 3 >= AC_BUFFSIZE)
			{
				SV_AntiCheat_Unexpected_Disconnect ();
				return;
			}

			ping_pending = true;

			*(unsigned short *)(acSendBuffer + acSendBufferLen) = 1;
			acSendBufferLen += 2;
			acSendBuffer[acSendBufferLen++] = Q2S_PING;
		}
	}

	for (cl = svs.clients; cl < svs.clients + maxclients->intvalue; cl++)
	{
		if (cl->state == cs_spawning)
		{
			if (cl->anticheat_query_sent == ANTICHEAT_QUERY_SENT && (unsigned)(curtime - cl->anticheat_query_time) > 5000)
			{
				Com_Printf ("ANTICHEAT WARNING: Query timed out for %s, possible network problem.\n", LOG_SERVER|LOG_ANTICHEAT|LOG_WARNING, cl->name);
				cl->anticheat_valid = false;
				SV_ClientBegin (cl);
				continue;
			}
		}
		else if (cl->state == cs_spawned)
		{
			if (cl->anticheat_nag_time && (unsigned)(curtime - cl->anticheat_nag_time) >= sv_anticheat_nag_defer->intvalue * 1000)
			{
				SV_AntiCheat_Nag (cl);
			}
		}
	}
}

void SVCmd_SVACInvalidate_f (void)
{
	client_t	*cl;

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	for (cl = svs.clients; cl < svs.clients + maxclients->intvalue; cl++)
	{
		if (cl->state > cs_connected)
			SV_AntiCheat_Disconnect_Client (cl);
	}

	Com_Printf ("All clients marked as invalid.\n", LOG_GENERAL);
}

void SVCmd_SVACUpdate_f (void)
{
	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	SV_AntiCheat_SendFileAndCvarChecks ();
	Com_Printf ("Anticheat configuration updated.\n", LOG_GENERAL);
}

//FIXME duplicated code
void SVCmd_SVACList_f (void)
{
	client_t	*cl;
	const char	*substring;

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	substring = Cmd_Argv (1);

	Com_Printf (
		"+----------------+--------+-----+------+\n"
		"|  Player Name   |AC Valid|Files|Client|\n"
		"+----------------+--------+-----+------+\n", LOG_GENERAL);

	for (cl = svs.clients; cl < svs.clients + maxclients->intvalue; cl++)
	{
		if (cl->state < cs_spawned)
			continue;

		if (!substring[0] || strstr (cl->name, substring))
		{
			if (cl->anticheat_valid)
			{
				int	index;
				index = cl->anticheat_client_type;
				if (index >= sizeof(anticheat_client_names) / sizeof(anticheat_client_names[0]))
					index = 0;
				Com_Printf ("|%-16s|%s| %3d |%-6s|\n", LOG_GENERAL,
					cl->name, "   yes  ", cl->anticheat_file_failures, anticheat_client_names[index]);
			}
			else
			{
				Com_Printf ("|%-16s|%s| N/A | N/A  |\n", LOG_GENERAL,
					cl->name, cl->anticheat_required == ANTICHEAT_EXEMPT ? " exempt " : "   NO   ");
			}
		}
	}

	Com_Printf ("+----------------+--------+-----+------+\n", LOG_GENERAL);
}

//FIXME duplicated code
void SVCmd_SVACInfo_f (void)
{
	ptrdiff_t			clientID;
	const char			*substring;
	const char			*filesubstring;
	client_t			*cl;
	linkednamelist_t	*bad;

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n", LOG_GENERAL);
		return;
	}

	if (Cmd_Argc() == 1)
	{
		Com_Printf ("Usage: svacinfo [substring|id]\n", LOG_GENERAL);
		return;
	}
	else
	{
		substring = Cmd_Argv (1);
		filesubstring = Cmd_Argv (2);

		clientID = -1;

		if (StringIsNumeric (substring))
		{
			clientID = atoi (substring);
			if (clientID >= maxclients->intvalue || clientID < 0)
			{
				Com_Printf ("Invalid client ID.\n", LOG_GENERAL);
				return;
			}
		}
		else
		{
			for (cl = svs.clients; cl < svs.clients + maxclients->intvalue; cl++)
			{
				if (cl->state < cs_spawned)
					continue;

				if (strstr (cl->name, substring))
				{
					clientID = cl - svs.clients;
					break;
				}
			}
		}

		if (clientID == -1)
		{
			Com_Printf ("Player not found.\n", LOG_GENERAL);
			return;
		}

		cl = &svs.clients[clientID];
		if (cl->state < cs_spawned)
		{
			Com_Printf ("Player is not active.\n", LOG_GENERAL);
			return;
		}
	}

	bad = &cl->anticheat_bad_files;

	Com_Printf ("File check failures for %s:\n", LOG_GENERAL, cl->name);
	while (bad->next)
	{
		bad = bad->next;
		if (!filesubstring[0] || strstr (bad->name, filesubstring))
			Com_Printf ("%s\n", LOG_GENERAL, bad->name);
	}
}

void SV_AntiCheat_Run (void)
{
	struct timeval	tv;
	fd_set			set;
	int				ret;

	if (retryTime && time(NULL) >= retryTime)
	{
		Com_Printf ("ANTICHEAT: Attempting to reconnect to anticheat server...\n", LOG_SERVER|LOG_ANTICHEAT);
		SV_AntiCheat_Connect();
	}

	if (acSocket == 0)
		return;

	FD_ZERO (&set);
	FD_SET (acSocket, &set);

	tv.tv_sec = tv.tv_usec = 0;

	if (connect_pending)
	{
		fd_set	wset, eset;
		FD_ZERO (&wset);
		FD_ZERO (&eset);
		FD_SET (acSocket, &wset);
		FD_SET (acSocket, &eset);
		ret = select ((int)acSocket + 1, NULL, &wset, &eset, &tv);
		if (ret == 1)
		{
			int		exception_occured = 0;
			int		connect_occured = 0;
#ifdef LINUX
			int		linux_socket_implementation_can_lick_my_nuts;
			socklen_t	this_shit_sucks;
			//fucking linux can't handle select like every other half implemented OS, nooo...
			this_shit_sucks = sizeof(linux_socket_implementation_can_lick_my_nuts);
			getsockopt (acSocket, SOL_SOCKET, SO_ERROR, &linux_socket_implementation_can_lick_my_nuts, &this_shit_sucks);
			if (linux_socket_implementation_can_lick_my_nuts == 0)
				connect_occured = 1;
			else
				exception_occured = 1;
#else
			if (FD_ISSET (acSocket, &eset))
				exception_occured = 1;
			else if (FD_ISSET (acSocket, &wset))
				connect_occured = 1;
#endif
			if (exception_occured)
			{
				retryTime = time(NULL) + retryBackOff;
				Com_Printf ("ANTICHEAT: Server connection failed. Retrying in %d seconds.\n", LOG_SERVER|LOG_ANTICHEAT, retryBackOff);
				closesocket (acSocket);
				acSocket = 0;
				retryBackOff += 5;
				return;
			}
			else if (connect_occured)
			{
				Com_Printf ("ANTICHEAT: Connected to anticheat server!\n", LOG_SERVER|LOG_ANTICHEAT);
				connect_pending = false;
				retryTime = 0;
				SV_AntiCheat_Hello ();
			}
		}
		else if (ret == -1)
		{
			retryTime = time(NULL) + retryBackOff;
			Com_Printf ("ANTICHEAT: Server connection failed. Retrying in %d seconds.\n", LOG_SERVER|LOG_ANTICHEAT, retryBackOff);
			closesocket (acSocket);
			acSocket = 0;
			retryBackOff += 5;
			return;
		}
		return;
	}

	SV_AntiCheat_CheckTimeOuts ();

	for (;;)
	{
		FD_ZERO (&set);
		FD_SET (acSocket, &set);
		tv.tv_sec = tv.tv_usec = 0;
		ret = select ((int)acSocket + 1, &set, NULL, NULL, &tv);
		if (ret < 0)
		{
			SV_AntiCheat_Unexpected_Disconnect ();
			return;
		}
		else if (ret == 1)
		{
			if (!expectedLength)
			{
				ret = recv (acSocket, packetBuffer + packetLen, 2 - packetLen, 0);
				if (ret <= 0)
				{
					SV_AntiCheat_Unexpected_Disconnect ();
					return;
				}

				packetLen += ret;

				if (packetLen == 2)
				{
					expectedLength = *(unsigned short *)&packetBuffer[0];		
					packetLen = 0;

					if (expectedLength > sizeof(packetBuffer))
					{
						Com_Printf ("ANTICHEAT WARNING: Expected packet length %d exceeds buffer size %d!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT, expectedLength, (int)sizeof(packetBuffer));
						expectedLength = sizeof(packetBuffer);
					}
				}
			}
			else
			{
				ret = recv (acSocket, packetBuffer + packetLen, expectedLength - packetLen, 0);
				if (ret <= 0)
				{
					SV_AntiCheat_Unexpected_Disconnect ();
					return;
				}

				packetLen += ret;

				if (packetLen == expectedLength)
				{
					SV_AntiCheat_ParseBuffer ();
					packetLen = 0;
					expectedLength = 0;
				}
			}
		}
		else
			break;
	}

	if (acSendBufferLen)
	{
		for (;;)
		{
			FD_ZERO (&set);
			FD_SET (acSocket, &set);
			tv.tv_sec = tv.tv_usec = 0;

			ret = select ((int)acSocket + 1, NULL, &set, NULL, &tv);
			if (ret < 0)
			{
				SV_AntiCheat_Unexpected_Disconnect ();
				return;
			}
			else if (ret == 1)
			{
				ret = send ((int)acSocket, acSendBuffer, (int)acSendBufferLen, 0);
				if (ret <= 0)
				{
					SV_AntiCheat_Unexpected_Disconnect ();
					return;
				}
				memmove (acSendBuffer, acSendBuffer + ret, acSendBufferLen - ret);
				acSendBufferLen -= ret;
			}
			else
				break;
			
			if (!acSendBufferLen)
				break;
		}
	}
}

//yuck, but necessary for synchronizing startup
void SV_AntiCheat_WaitForInitialConnect (void)
{
	int	attempts;

	if (!acSocket)
		return;

	attempts = 0;

	while (acSocket && !anticheat_ready)
	{
		SV_AntiCheat_Run ();
		Sys_Sleep (1);

		//something is wrong, abort.
		if (++attempts == 5000)
			break;
	}
}

qboolean SV_AntiCheat_Disconnect_Client (client_t *cl)
{
	int		num;

	cl->anticheat_query_sent = ANTICHEAT_QUERY_UNSENT;
	cl->anticheat_valid = false;

	if (!anticheat_ready)
		return false;

	if (acSendBufferLen + 7 >= AC_BUFFSIZE)
	{
		Com_Printf ("ANTICHEAT WARNING: Anticheat send buffer length exceeded in SV_AntiCheat_Disconnect!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
		return false;
	}

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 9;
	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_CLIENTDISCONNECT;

	num = (int)(cl - svs.clients);

	memcpy (acSendBuffer + acSendBufferLen, &num, sizeof(num));
	acSendBufferLen += sizeof(num);

	memcpy (acSendBuffer + acSendBufferLen, &cl->challenge, sizeof(cl->challenge));
	acSendBufferLen += sizeof(cl->challenge);

	return true;
}

qboolean SV_AntiCheat_Challenge (netadr_t *from, client_t *cl)
{
	int		num;

	if (!anticheat_ready)
		return false;

	if (acSendBufferLen + 17 >= AC_BUFFSIZE)
	{
		Com_Printf ("ANTICHEAT WARNING: Anticheat send buffer length exceeded in SV_AntiCheat_Challenge!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
		return false;
	}

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 15;
	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_REQUESTCHALLENGE;

	memcpy (acSendBuffer + acSendBufferLen, from->ip, sizeof(from->ip));
	acSendBufferLen += sizeof(from->ip);

	memcpy (acSendBuffer + acSendBufferLen, &from->port, sizeof(from->port));
	acSendBufferLen += sizeof(from->port);

	num = (int)(cl - svs.clients);

	memcpy (acSendBuffer + acSendBufferLen, &num, sizeof(num));
	acSendBufferLen += sizeof(num);

	memcpy (acSendBuffer + acSendBufferLen, &cl->challenge, sizeof(cl->challenge));
	acSendBufferLen += sizeof(cl->challenge);
	return true;
}

qboolean SV_AntiCheat_QueryClient (client_t *cl)
{
	int		num;

	cl->anticheat_query_sent = ANTICHEAT_QUERY_SENT;
	cl->anticheat_query_time = curtime;

	if (!anticheat_ready)
		return false;

	if (sv_anticheat_nag_time->intvalue)
		cl->anticheat_nag_time = curtime;

	if (acSendBufferLen + 11 >= AC_BUFFSIZE)
	{
		Com_Printf ("ANTICHEAT WARNING: Anticheat send buffer length exceeded in SV_AntiCheat_QueryClient!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
		return false;
	}

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 9;
	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_QUERYCLIENT;

	num = (int)(cl - svs.clients);

	memcpy (acSendBuffer + acSendBufferLen, &num, sizeof(num));
	acSendBufferLen += sizeof(num);

	memcpy (acSendBuffer + acSendBufferLen, &cl->challenge, sizeof(cl->challenge));
	acSendBufferLen += sizeof(cl->challenge);

	return true;
}

const char *SV_AntiCheat_CheckToken (const char *token)
{
	linkednamelist_t	*t;

	t = &anticheat_tokens;

	while (t->next)
	{
		t = t->next;

		if (!strcmp (t->name, token))
			return t->name;
	}

	return NULL;
}

void SV_AntiCheat_Disconnect (void)
{
	if (!acSocket)
		return;

	closesocket (acSocket);
	acSocket = 0;
	retryTime = 0;
	anticheat_ready = false;
	ping_pending = false;

	Cvar_FullSet ("anticheat", "0", CVAR_NOSET);
}

qboolean SV_AntiCheat_IsConnected (void)
{
	return anticheat_ready;
}

qboolean SV_AntiCheat_Connect (void)
{
	struct hostent		*h;
	struct sockaddr_in	bindAddress;
	const char			*ip;
	const unsigned long	_true = 1;

	if (acSocket)
		return true;

	h = gethostbyname (sv_anticheat_server_address->string);

	if (!h)
	{
		Com_Printf ("ANTICHEAT: Unable to lookup anticheat server address '%s'. Retrying in %d seconds.\n", LOG_SERVER|LOG_ANTICHEAT, sv_anticheat_server_address->string, retryBackOff);
		retryTime = time(NULL) + retryBackOff;
		retryBackOff += 60;
		return false;
	}

	acSocket = socket (AF_INET, SOCK_STREAM, 0);

	if (acSocket == SOCKET_ERROR)
		Com_Error (ERR_DROP, "SV_AntiCheat_Connect: socket");

	if (!dedicated->intvalue)
	{
		Com_Printf ("ANTICHEAT: Anticheat is only supported on dedicated servers, disabling.\n", LOG_SERVER|LOG_ANTICHEAT);
		SV_AntiCheat_Disconnect ();
		return false;
	}

	if (ioctlsocket (acSocket, FIONBIO, (u_long *)&_true) == -1)
		Com_Error (ERR_DROP, "SV_AntiCheat_Connect: ioctl");

	setsockopt (acSocket, SOL_SOCKET, SO_KEEPALIVE, (const char *)&_true, sizeof(_true));

	memset (&bindAddress.sin_zero, 0, sizeof(bindAddress.sin_zero));
	bindAddress.sin_family = AF_INET;
	bindAddress.sin_port = 0;

	ip = Cvar_VariableString ("ip");
	if (ip[0] && Q_stricmp (ip, "localhost"))
		NET_StringToSockaddr (ip, (struct sockaddr *)&bindAddress);
	else
		bindAddress.sin_addr.s_addr = INADDR_ANY;
	
	if (bind (acSocket, (const struct sockaddr *)&bindAddress, sizeof(bindAddress)) == SOCKET_ERROR)
		Com_Error (ERR_DROP, "SV_AntiCheat_Connect: couldn't bind to %s", ip);

	memset (&acServer.sin_zero, 0, sizeof(acServer.sin_zero));
	acServer.sin_family = AF_INET;
	acServer.sin_port = htons (27910);

	memcpy (&acServer.sin_addr, h->h_addr_list[0], sizeof(acServer.sin_addr));

	retryTime = 0;

	connect_pending = true;
	connect (acSocket, (const struct sockaddr *)&acServer, sizeof(acServer));

	packetLen = 0;
	expectedLength = 0;

	anticheat_ready = false;

	return true;
}

#endif
