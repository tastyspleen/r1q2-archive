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
	#include <sys/time.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
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

qboolean SV_AntiCheat_Connect (void);
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

int		acSendBufferLen;
byte	acSendBuffer[AC_BUFFSIZE];

enum acserverbytes_e
{
	ACS_BAD,
	ACS_CHALLENGE,
	ACS_VIOLATION,
};

enum q2serverbytes_e
{
	Q2S_BAD,
	Q2S_VERSION,
	Q2S_PREF,
	Q2S_FILEHASHES,
	Q2S_CVARLOCKS,
	Q2S_REQUESTCHALLENGE,
};

static void SV_AntiCheat_Unexpected_Disconnect (void)
{
	closesocket (acSocket);
	acSocket = 0;

	retryBackOff = DEFAULT_BACKOFF;
	retryTime = time(NULL) + retryBackOff;

	Com_Printf ("ANTICHEAT WARNING: Lost connection to anticheat server! Will attempt to reconnect in %d seconds.\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT, retryBackOff);
}

static void SV_AntiCheat_ParseBuffer (void)
{
	byte	 *buff;
	int		bufflen;

	if (!packetLen)
		return;

	buff = packetBuffer;
	bufflen = packetLen;

	switch (buff[0])
	{
		case ACS_VIOLATION:
			break;
	}
}

static void SV_AntiCheat_Hello (void)
{
	unsigned short	len, hostlen, verlen;
	const char		*host;
	const char		*ver;

	acSendBufferLen = 1;
	acSendBuffer[0] = '\x02';

	host = hostname->string;
	ver = R1Q2_VERSION_STRING;

	hostlen = strlen(host);
	verlen = strlen(ver);

	len = 7 + hostlen + verlen;

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = len;
	acSendBufferLen += 2;

	acSendBuffer[acSendBufferLen++] = Q2S_VERSION;

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
}

void SV_AntiCheat_Run (void)
{
	struct timeval	tv;
	struct fd_set	set;
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
		struct fd_set wset, eset;
		FD_ZERO (&wset);
		FD_ZERO (&eset);
		FD_SET (acSocket, &wset);
		FD_SET (acSocket, &eset);
		ret = select (acSocket + 1, NULL, &wset, &eset, &tv);
		if (ret == 1)
		{
			if (FD_ISSET (acSocket, &wset))
			{
				Com_Printf ("ANTICHEAT: Connected to anticheat server!\n", LOG_SERVER|LOG_ANTICHEAT);
				connect_pending = false;
				retryTime = 0;
				retryBackOff = DEFAULT_BACKOFF;
				SV_AntiCheat_Hello ();
			}
			else if (FD_ISSET (acSocket, &eset))
			{
				retryTime = time(NULL) + retryBackOff;
				Com_Printf ("ANTICHEAT: Server connection failed. Retrying in %d seconds.\n", LOG_SERVER|LOG_ANTICHEAT, retryBackOff);
				closesocket (acSocket);
				acSocket = 0;
				retryBackOff += 5;
				return;
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

	ret = select (acSocket + 1, &set, NULL, NULL, &tv);
	if (ret < 0)
	{
		SV_AntiCheat_Unexpected_Disconnect ();
	}
	else if (ret == 1)
	{
		if (!expectedLength)
		{
			ret = recv (acSocket, packetBuffer + packetLen, 2 - packetLen, 0);
			if (ret <= 0)
				SV_AntiCheat_Unexpected_Disconnect ();

			packetLen += ret;

			if (packetLen == 2)
			{
				expectedLength = *(unsigned short *)&packetBuffer[0];		
				packetLen = 0;

				if (expectedLength > sizeof(packetBuffer))
				{
					Com_Printf ("ANTICHEAT WARNING: Expected packet length %d exceeds buffer size %d!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT, expectedLength, sizeof(packetBuffer));
					expectedLength = sizeof(packetBuffer);
				}
			}
		}
		else
		{
			ret = recv (acSocket, packetBuffer, expectedLength - packetLen, 0);
			if (ret <= 0)
				SV_AntiCheat_Unexpected_Disconnect ();

			packetLen += ret;

			if (packetLen == expectedLength)
			{
				SV_AntiCheat_ParseBuffer ();
				packetLen = 0;
				expectedLength = 0;
			}
		}
	}

	if (acSendBufferLen)
	{
		FD_ZERO (&set);
		FD_SET (acSocket, &set);

		tv.tv_sec = tv.tv_usec = 0;

		ret = select (acSocket + 1, NULL, &set, NULL, &tv);
		if (ret < 0)
		{
			SV_AntiCheat_Unexpected_Disconnect ();
		}
		else if (ret == 1)
		{
			ret = send (acSocket, acSendBuffer, acSendBufferLen, 0);
			if (ret <= 0)
				SV_AntiCheat_Unexpected_Disconnect ();
			memmove (acSendBuffer, acSendBuffer + ret, acSendBufferLen - ret);
			acSendBufferLen -= ret;
		}
	}
}

void SV_AntiCheat_Challenge (netadr_t *from, unsigned challenge)
{
	if (acSendBufferLen + 13 > AC_BUFFSIZE)
	{
		Com_Printf ("ANTICHEAT WARNING: Anticheat send buffer length exceeded in SV_AntiCheat_Challenge!\n", LOG_WARNING|LOG_SERVER|LOG_ANTICHEAT);
		return;
	}

	*(unsigned short *)(acSendBuffer + acSendBufferLen) = 11;

	acSendBuffer[acSendBufferLen++] = Q2S_REQUESTCHALLENGE;

	memcpy (acSendBuffer + acSendBufferLen, from->ip, sizeof(from->ip));
	acSendBufferLen += sizeof(from->ip);

	memcpy (acSendBuffer + acSendBufferLen, &from->port, sizeof(from->port));
	acSendBufferLen += sizeof(from->port);

	memcpy (acSendBuffer + acSendBufferLen, &challenge, sizeof(challenge));
	acSendBufferLen += sizeof(challenge);
}

void SV_AntiCheat_Disconnect (void)
{
	if (!acSocket)
		return;

	closesocket (acSocket);
	retryTime = 0;
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

	return true;
}

#endif
