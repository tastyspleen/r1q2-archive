// net_wins.c

#include "../qcommon/qcommon.h"

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

#ifdef NeXT
#include <libc.h>
#endif

unsigned int net_inittime;
unsigned long long net_total_in;
unsigned long long net_total_out;
unsigned long long net_packets_in;
unsigned long long net_packets_out;

int			server_port;
//netadr_t	net_local_adr;

#define	LOOPBACK	0x7f000001

#define	MAX_LOOPBACK	4

typedef struct
{
	byte	data[MAX_MSGLEN];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

loopback_t	loopbacks[2];
int			ip_sockets[2];

int NET_Socket (char *net_interface, int port);
char *NET_ErrorString (void);

//Aiee...
#include "../qcommon/net_common.c"

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
/*qboolean	NET_StringToSockaddr (char *s, struct sockaddr *sadr)
{
	int		isip = 0;
	char	*p;
	struct hostent	*h;
	char	*colon;
	char	copy[128];
	
	memset (sadr, 0, sizeof(*sadr));

	//r1: better than just the first digit for ip validity :)
	p = s;
	while (*p) {
		if (*p == '.') {
			isip++;
		} else if (*p == ':') {
			break;
		} else if (!isdigit(*p)) {
			isip = 0;
			break;
		}
		p++;
	}

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	strncpy (copy, s, sizeof(copy)-1);

	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));	
		}
	
	if (isip)
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if (! (h = gethostbyname(copy)) )
			return 0;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}*/

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
/*qboolean	NET_StringToAdr (char *s, netadr_t *a)
{
	struct sockaddr_in sadr;
	
	if (!strcmp (s, "localhost"))
	{
		memset (a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr (s, (struct sockaddr *)&sadr))
		return false;
	
	SockadrToNetadr (&sadr, a);

	return true;
}*/

void Net_Stats_f (void)
{
	int now = time(0);
	int diff = now - net_inittime;

	Com_Printf ("Network up for %i seconds.\n"
				"%llu bytes in %llu packets received (av: %i kbps)\n"
				"%llu bytes in %llu packets sent (av: %i kbps)\n",
				
				diff,
				net_total_in, net_packets_in, (int)(((net_total_in * 8) / 1024) / diff),
				net_total_out, net_packets_out, (int)((net_total_out * 8) / 1024) / diff);
}

/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/


//=============================================================================

int	NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret;
	struct sockaddr_in	from;
	unsigned int		fromlen;
	int		net_socket;
	int		err;

#ifndef DEDICATED_ONLY
	if (NET_GetLoopPacket (sock, net_from, net_message))
		return 1;
#endif

	net_socket = ip_sockets[sock];

	if (!net_socket)
		return 0;

	fromlen = sizeof(from);
	ret = recvfrom (net_socket, net_message->data, net_message->maxsize
		, 0, (struct sockaddr *)&from, &fromlen);

	if (ret == -1)
	{
		err = errno;

		if (err == EWOULDBLOCK)
			return 0;
		if (err == ECONNREFUSED) {
			SockadrToNetadr (&from, net_from);
			Com_Printf ("NET_GetPacket: %s from %s", NET_ErrorString(), NET_AdrToString (net_from));
			return -1;
		}
		Com_Printf ("NET_GetPacket: %s", NET_ErrorString());
		return 0;
	}

	net_packets_in++;
	net_total_in += ret;

	SockadrToNetadr (&from, net_from);

	if (ret == net_message->maxsize)
	{
		Com_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return 0;
	}

	net_message->cursize = ret;
	
	return 1;
}

//=============================================================================

int NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t *to)
{
	int		ret;
	struct sockaddr_in	addr;
	int		net_socket;

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
		return 1;
	}
#endif
	else if (to->type == NA_BROADCAST)
	{
		net_socket = ip_sockets[sock];
		if (!net_socket)
			return 0;
	}
	else
	{
		Com_Error (ERR_FATAL, "NET_SendPacket: bad address type");
		return 0;
	}

	NetadrToSockadr (to, &addr);

	ret = sendto (net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr) );
	if (ret == -1)
	{
		Com_Printf ("NET_SendPacket to %s: ERROR: %s\n", NET_AdrToString(to), NET_ErrorString());
		return 0;
	}

	net_packets_out++;
	net_total_out += ret;
	return 1;
}

//=============================================================================


void NET_CloseSocket (int s)
{
	shutdown (s, 0x02);
	close (s);
}

/*
====================
NET_OpenIP
====================
*/
void NET_OpenIP (int flags)
{
	cvar_t	*port, *ip;

	port = Cvar_Get ("port", va("%i", PORT_SERVER), CVAR_NOSET);
	ip = Cvar_Get ("ip", "localhost", CVAR_NOSET);

	server_port = (int)port->value;

	net_inittime = time(0);

	if (flags & NET_SERVER)
	{
		if (!ip_sockets[NS_SERVER])
			ip_sockets[NS_SERVER] = NET_Socket (ip->string, port->value);
	}

	if (!ip_sockets[NS_CLIENT])
		ip_sockets[NS_CLIENT] = NET_Socket (ip->string, PORT_ANY);

	net_total_in = net_packets_in = net_total_out = net_packets_out = 0;
}


/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/
/*void	NET_Config (qboolean multiplayer)
{
	int		i;

	if (!multiplayer)
	{	// shut down any existing sockets
		for (i=0 ; i<2 ; i++)
		{
			if (ip_sockets[i])
			{
				close (ip_sockets[i]);
				ip_sockets[i] = 0;
			}
		}
	}
	else
	{	// open sockets
		NET_OpenIP ();
	}
}*/

int	NET_Config (int toOpen)
{
	int		i;
	static	int	old_config;

	i = old_config;

	if (old_config == toOpen)
		return i;

	old_config |= toOpen;

	if (toOpen == NET_NONE)
	{
		if (ip_sockets[NS_CLIENT])
		{
			close (ip_sockets[NS_CLIENT]);
			ip_sockets[NS_CLIENT] = 0;
		}

		if (ip_sockets[NET_SERVER])
		{
			close (ip_sockets[NET_SERVER]);
			ip_sockets[NET_SERVER] = 0;
		}

		old_config = NET_NONE;
	}

	NET_OpenIP (toOpen);

	return i;
}

//===================================================================


/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
	Cmd_AddCommand ("net_stats", Net_Stats_f);
}


/*
====================
NET_Socket
====================
*/
int NET_Socket (char *net_interface, int port)
{
	int newsocket;
	struct sockaddr_in address;
	qboolean _true = true;
	int	i = 1;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		Com_Error (ERR_DROP, "UDP_OpenSocket: Couldn't make socket: %s", NET_ErrorString());
		return 0;
	}

	// make it non-blocking
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
	{
		Com_Error (ERR_DROP, "UDP_OpenSocket: Couldn't make non-blocking: %s", NET_ErrorString());
		return 0;
	}

	// make it broadcast capable
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) == -1)
	{
		Com_Error (ERR_DROP, "UDP_OpenSocket: Couldn't set SO_BROADCAST: %s", NET_ErrorString());
		return 0;
	}

	if (!net_interface || !net_interface[0] || !Q_stricmp(net_interface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr (net_interface, (struct sockaddr *)&address);

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);

	address.sin_family = AF_INET;

	if( bind (newsocket, (void *)&address, sizeof(address)) == -1)
	{
		close (newsocket);
		Com_Error (ERR_DROP, "UDP_OpenSocket: Couldn't bind to port %d: %s", port, NET_ErrorString());
		return 0;
	}

	return newsocket;
}


/*
====================
NET_Shutdown
====================
*/
void	NET_Shutdown (void)
{
	NET_Config (NET_NONE);	// close sockets
}


/*
====================
NET_ErrorString
====================
*/
char *NET_ErrorString (void)
{
	int		code;

	code = errno;
	return strerror (code);
}

// sleeps msec or until net socket is ready
void NET_Sleep(int msec)
{
    struct timeval timeout;
	fd_set	fdset;
	extern cvar_t *dedicated;
	//extern qboolean stdin_active;

	if (!ip_sockets[NS_SERVER] || (dedicated && !dedicated->value))
		return; // we're not a server, just run full speed

	FD_ZERO(&fdset);
	//if (stdin_active)
	//	FD_SET(0, &fdset); // stdin is processed too
	FD_SET(ip_sockets[NS_SERVER], &fdset); // network socket
	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	select(ip_sockets[NS_SERVER]+1, &fdset, NULL, NULL, &timeout);
}
