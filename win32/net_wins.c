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
// net_wins.c

#define WIN32_LEAN_AND_MEAN
//#include "winsock.h"
//#include "wsipx.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../qcommon/qcommon.h"

static int net_inittime;

static uint64 net_total_in;
static uint64 net_total_out;
static uint64 net_packets_in;
static uint64 net_packets_out;

//cvar_t		*net_shownet;
//static cvar_t	*noudp;
//static cvar_t	*noipx;

static cvar_t	*net_rcvbuf;
static cvar_t	*net_sndbuf;

static SOCKET		ip_sockets[2];
int			server_port;
//int			ipx_sockets[2];

char *NET_ErrorString (void);

//Aiee...
#include "../qcommon/net_common.c"

//=============================================================================

//r1: for some reason attempting to macroize this fails badly..
/*__inline void NetadrToSockadr (netadr_t *a, struct sockaddr *s)
{
	memset (s, 0, sizeof(*s));

	if (a->type == NA_IP)
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
		((struct sockaddr_in *)s)->sin_port = a->port;
	}
	else if (a->type == NA_BROADCAST)
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
	}
}*/


/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/

//=============================================================================

int	NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret;
	struct sockaddr from;
	int		fromlen;
	int		err;

#ifndef DEDICATED_ONLY
	if (NET_GetLoopPacket (sock, net_from, net_message))
		return 1;
#endif

	if (!ip_sockets[sock])
		return 0;

	fromlen = sizeof(from);
	ret = recvfrom (ip_sockets[sock], (char *)net_message->data, net_message->maxsize
		, 0, (struct sockaddr *)&from, &fromlen);

	if (ret == -1)
	{
		err = WSAGetLastError();

		if (err == WSAEWOULDBLOCK)// || (err == WSAECONNRESET && Com_ServerState()))
			return 0;

		if (err == WSAECONNRESET)
		{
			if (net_ignore_icmp->intvalue)
			{
				return -2;
			}
			else
			{
				SockadrToNetadr (&from, net_from);
				return -1; 
			}
		}
#ifndef NO_SERVER
		if (dedicated->intvalue)	// let dedicated servers continue after errors
			Com_Printf ("NET_GetPacket: %s\n", LOG_NET, NET_ErrorString());
		else
#endif

			//r1: WSAEMSGSIZE can be caused by someone UDP packeting a client.
			//FIXME: somehow get the current clients connected server address
			//       here and if any packets that didn't originate from the server
			//       cause errors, silently ignore them.
			if (err != WSAEMSGSIZE)
				Com_Printf ("WARNING! NET_GetPacket: %s\n", LOG_NET, NET_ErrorString());
			return 0;
	}

	net_packets_in++;
	net_total_in += ret;

	SockadrToNetadr (&from, net_from);

	if (ret == net_message->maxsize)
	{
		Com_Printf ("Oversize packet from %s\n", LOG_NET, NET_AdrToString (net_from));
		return 0;
	}

	net_message->cursize = ret;
	return 1;
}

//=============================================================================

int NET_SendPacket (netsrc_t sock, int length, const void *data, netadr_t *to)
{
//	char *z;
	int					ret;
	struct sockaddr_in	addr;
	SOCKET				net_socket;

	if (to->type == NA_IP)
	{
		net_socket = ip_sockets[sock];
		if (!net_socket)
			return 0;
	}
#ifndef DEDICATED_ONLY
	else if ( to->type == NA_LOOPBACK )
	{
		NET_SendLoopPacket (sock, length, data);
		return 0;
	}
#endif
	else if (to->type == NA_BROADCAST)
	{
		net_socket = ip_sockets[sock];
		if (!net_socket)
			return 0;
	}
	else
		Com_Error (ERR_FATAL, "NET_SendPacket: bad address type");

	if (sock == NS_CLIENT && net_proxy_active)
		to = &net_proxy_addr;

	NetadrToSockadr (to, &addr);

	ret = sendto (net_socket, data, length, 0, (const struct sockaddr *)&addr, sizeof(addr) );
	
	if (ret == -1)
	{
		int err = WSAGetLastError();

		// wouldblock is silent
		// r1: add WSAEINTR too since some weird shit makes that fail :/
		if (err == WSAEWOULDBLOCK || err == WSAEINTR)
			return 0;

		// some PPP links dont allow broadcasts
		if ((err == WSAEADDRNOTAVAIL) && ((to->type == NA_BROADCAST)))
			return 0;

#ifndef NO_SERVER
		if (dedicated->intvalue)	// let dedicated servers continue after errors
		{
			//r1: this error is "normal" in Win2k TCP/IP stack, don't bother spamming server
			//    console with it.
			if (err == WSAECONNRESET)
			{
				if (net_ignore_icmp->intvalue)
					return 0;
				else
					return -1;
			}
				//Com_Printf ("NET_SendPacket ERROR: %s\n", NET_ErrorString());
		}
		else
		{
#endif
			if (err == WSAEADDRNOTAVAIL)
			{
				Com_DPrintf ("NET_SendPacket Warning: %s : %s\n", NET_ErrorString(), NET_AdrToString (to));
			}

			//r1: ignore "errors" from connectionless info packets (FUCKING UGLY HACK)
			//    if the first 4 bytes are connectionless and len=11 (ÿÿÿÿinfo 34) ignore.

			//r1: also ignore 10053 (connection reset by peer) messages if we are running
			//    a server. 2k/xp ip stack seems to send a bunch of these if a client disconnects
			//    - if we are a listen server then we would Com_Error out at this point without
			//    this fix (or hack if you prefer).
			else {
				if (to->type != NA_BROADCAST && !(sock == NS_CLIENT && length == 12 && *(int *)data == -1) &&
					!(sock == NS_SERVER && err == WSAECONNRESET))
					Com_Error (ERR_NET, "NET_SendPacket ERROR: %s", NET_ErrorString());
			}
		}
#ifndef NO_SERVER
	}
#endif
	net_packets_out++;
	net_total_out += ret;
	return 1;
}


//=============================================================================


/*
====================
NET_Socket
====================
*/
int NET_IPSocket (char *net_interface, int port)
{
	SOCKET				newsocket;
	struct sockaddr_in	address;
	int					i;
	int					j;
	int					x = sizeof(i);
	int					err;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		err = WSAGetLastError();
		if (err != WSAEAFNOSUPPORT)
			Com_Printf ("WARNING: UDP_OpenSocket: socket: %s\n", LOG_NET, NET_ErrorString());
		return 0;
	}

	i = net_rcvbuf->intvalue * 1024;
	if (i)
	{
		setsockopt (newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof(i));
		getsockopt (newsocket, SOL_SOCKET, SO_RCVBUF, (char *)&j, &x);
		if (i != j)
			Com_Printf ("WARNING: Setting SO_RCVBUF: wanted %d, got %d\n", LOG_NET, i, j);
	}

	i = net_sndbuf->intvalue * 1024;
	if (i)
	{
		setsockopt (newsocket, SOL_SOCKET, SO_SNDBUF, (char *)&i, sizeof(i));
		getsockopt (newsocket, SOL_SOCKET, SO_SNDBUF, (char *)&j, &x);
		if (i != j)
			Com_Printf ("WARNING: Setting SO_SNDBUF: wanted %d, got %d\n", LOG_NET, i, j);
	}

	i = 1;

	// make it non-blocking
	if (ioctlsocket (newsocket, FIONBIO, (u_long *)&_true) == -1)
	{
		Com_Printf ("UDP_OpenSocket: Couldn't make non-blocking: %s\n", LOG_NET, NET_ErrorString());
		return 0;
	}

	//setsockopt (sckRaw, IPPROTO_IP, IP_TTL, (char *)&stIPInfo.Ttl, sizeof(stIPInfo.Ttl));

	// make it broadcast capable
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) == -1)
	{
		Com_Printf ("UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", LOG_NET, NET_ErrorString());
		return 0;
	}

	//r1: set 'interactive' ToS
	i = 0x10;
	if (setsockopt(newsocket, IPPROTO_IP, IP_TOS, (char *)&i, sizeof(i)) == -1)
		Com_Printf ("WARNING: UDP_OpenSocket: setsockopt IP_TOS: %s\n", LOG_NET, NET_ErrorString());

	if (!net_interface || !net_interface[0] || !Q_stricmp(net_interface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((uint16)port);

	address.sin_family = AF_INET;

	if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
	{
		Com_Printf ("UDP_OpenSocket: Couldn't bind to UDP port %d: %s\n", LOG_NET, port, NET_ErrorString());
		closesocket (newsocket);
		return 0;
	}

	return (int)newsocket;
}

void Net_Stats_f (void)
{
	unsigned int now = (unsigned int)time(NULL);
	unsigned int diff = now - net_inittime;

	Com_Printf ("Network up for %u seconds.\n"
				"%I64u bytes in %I64u packets received (av: %i kbps)\n"
				"%I64u bytes in %I64u packets sent (av: %i kbps)\n", LOG_NET,
				
				diff,
				net_total_in, net_packets_in, (int)(((net_total_in * 8) / 1024) / diff),
				net_total_out, net_packets_out, (int)((net_total_out * 8) / 1024) / diff);
}

// sleeps msec or until net socket is ready
#if 0
void NET_Sleep(int msec)
{
    struct timeval timeout;
	fd_set	fdset;

	extern cvar_t *dedicated;

	int i;

	if (!dedicated || !dedicated->intvalue)
		return; // we're not a server, just run full speed

	FD_ZERO(&fdset);
	i = 0;

	if (ip_sockets[NS_SERVER])
	{
		FD_SET(ip_sockets[NS_SERVER], &fdset); // network socket
		i = ip_sockets[NS_SERVER];
	}
	/*if (ipx_sockets[NS_SERVER]) {
		FD_SET(ipx_sockets[NS_SERVER], &fdset); // network socket
		if (ipx_sockets[NS_SERVER] > i)
			i = ipx_sockets[NS_SERVER];
	}*/
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(i+1, &fdset, NULL, NULL, &timeout);
}
#endif

//===================================================================


static WSADATA		winsockdata;

/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
//	WORD	wVersionRequested; 
	int		r;

//	wVersionRequested = MAKEWORD(1, 1);
	

	r = WSAStartup (MAKEWORD(1, 1), &winsockdata);

	if (r)
		Com_Error (ERR_FATAL,"Winsock initialization failed.");

	NET_Common_Init ();

	net_rcvbuf = Cvar_Get ("net_rcvbuf", "0", 0);
	net_sndbuf = Cvar_Get ("net_sndbuf", "0", 0);

	//r1: needed for pyroadmin hooks
#ifndef NO_SERVER
	if (dedicated->intvalue)
		NET_Config (NET_SERVER);
#endif

	Com_Printf("Winsock Initialized\n", LOG_NET);




	//noudp = Cvar_Get ("noudp", "0", CVAR_NOSET);
	//noipx = Cvar_Get ("noipx", "0", CVAR_NOSET);

	//net_shownet = Cvar_Get ("net_shownet", "0", 0);
}


/*
====================
NET_Shutdown
====================
*/
void	NET_Shutdown (void)
{
	NET_Config (NET_NONE);	// close sockets

	WSACleanup ();
}


/*
====================
NET_ErrorString
====================
*/

//r1: updated a bunch of messages with semi-understandable reasons
char *NET_ErrorString (void)
{
	int		code;

	code = WSAGetLastError ();
	switch (code)
	{

		//r1: this should NEVER happen. chances are some spyware or other bullshit is
		//    assing up the connection.
	case WSAEINTR: return "WSAEINTR: Interrupted function call (your TCP stack is likely broken/corrupt)";

	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES: Permission denied";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT: Network failure";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK: Resource temporarily unavailable";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE: Message too long";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE: Address already in use";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL: Cannot assign requested address";
	case WSAENETDOWN: return "WSAENETDOWN: Network is down";
	case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH: Host is unreachable";
	case WSAENETUNREACH: return "WSAENETUNREACH: No route to host";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET: Connection reset by peer";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED: Connection refused";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY: Network subsystem is unavailable";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return va("UNDEFINED ERROR %d", code);
	}
}