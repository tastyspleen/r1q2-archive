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

#include <linux/types.h>
#include <linux/errqueue.h>

#ifdef NeXT
#include <libc.h>
#endif

static unsigned int net_inittime;

static unsigned long long net_total_in;
static unsigned long long net_total_out;
static unsigned long long net_packets_in;
static unsigned long long net_packets_out;

int			server_port;
//netadr_t	net_local_adr;

static int			ip_sockets[2];

char *NET_ErrorString (void);

cvar_t	*net_no_recverr;

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
			((struct sockaddr_in *)sadr)->sin_port = htons((int16)atoi(colon+1));	
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
				"%llu bytes in %llu packets sent (av: %i kbps)\n", LOG_NET,
				
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

struct probehdr
{
	uint32_t ttl;
	struct timeval tv;
};

int	NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int 	ret;
	struct sockaddr_in	from;
	uint32	fromlen;
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
		//linux makes this needlessly complex, couldn't just return the source of the error in from, oh no...
		struct probehdr	rcvbuf;
		struct iovec	iov;
		struct msghdr	msg;
		struct cmsghdr	*cmsg;

		char		cbuf[1024];

		struct sock_extended_err *e;

		err = errno;

		memset (&rcvbuf, 0, sizeof(rcvbuf));

		iov.iov_base = &rcvbuf;
		iov.iov_len = sizeof (rcvbuf);

		memset (&from, 0, sizeof(from));

		msg.msg_name = (void *)&from;
		msg.msg_namelen = sizeof (from);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_flags = 0;
		msg.msg_control = cbuf;
		msg.msg_controllen = sizeof (cbuf);

		for (;;)
		{
			ret = recvmsg (net_socket, &msg, MSG_ERRQUEUE);
			if (ret == -1)
			{
				if (errno == EWOULDBLOCK || errno == EAGAIN)
				{
					if (err == EWOULDBLOCK || err == EAGAIN)
					{
						return 0;
					}
					else
					{
						errno = err;
						Com_Printf ("NET_GetPacket: %s\n", LOG_NET, NET_ErrorString());
						return 0;
					}
				}
				else
				{
					Com_DPrintf ("NET_GetPacket: recvmsg(): %s\n", NET_ErrorString());
					return 0;
				}
			}
			else if (!ret)
			{
				Com_DPrintf ("NET_GetPacket: recvmsg(): EOF\n");
				return 0;
			}

			errno = err;
			Com_DPrintf ("NET_GetPacket: Called recvmsg() for extended error details for %s\n", NET_ErrorString());

			//linux 2.2 (maybe others) fails to properly fill in the msg_name structure.
			Com_DPrintf ("(msgname) family %d, host: %s, port: %d, flags: %d\n", from.sin_family, inet_ntoa (from.sin_addr), from.sin_port, msg.msg_flags);

			e = NULL;

			for (cmsg = CMSG_FIRSTHDR (&msg); cmsg; cmsg = CMSG_NXTHDR (&msg, cmsg))
			{
				if (cmsg->cmsg_level == SOL_IP)
				{
					if (cmsg->cmsg_type == IP_RECVERR)
					{
						e = (struct sock_extended_err *) CMSG_DATA (cmsg);
					}
					else
						Com_DPrintf ("cmsg type = %d\n", cmsg->cmsg_type);
				}
			}

			if (!e)
			{
				Com_DPrintf ("NET_GetPacket: recvmsg(): no extended info available\n");
				continue;
			}

			if (e->ee_origin == SO_EE_ORIGIN_ICMP)
			{
				//for some unknown reason, the kernel zeroes out the port in SO_EE_OFFENDER, so this is pretty much useless
				struct sockaddr_in *sin = (struct sockaddr_in *)SO_EE_OFFENDER(e);
				Com_DPrintf ("(ICMP) family %d, host: %s, port: %d\n", sin->sin_family, inet_ntoa (sin->sin_addr), sin->sin_port);

				//but better than nothing if using  buggy kernel?
				if (from.sin_family == AF_UNSPEC)
				{
					memcpy (&from, sin, sizeof(from));
					//can't trust port, may be buggy kernel (again)
					from.sin_port = 0;
				}
			}
			else
			{
				Com_DPrintf ("NET_GetPacket: recvmsg(): error origin is %d\n", e->ee_origin);
				continue;
			}

			SockadrToNetadr (&from, net_from);

			switch (e->ee_errno)
			{
				case ECONNREFUSED:
				case EHOSTUNREACH:
				case ENETUNREACH:
					Com_Printf ("NET_GetPacket: %s from %s\n", LOG_NET, strerror(e->ee_errno), NET_AdrToString (net_from));
					if (net_ignore_icmp->intvalue)
						return 0;
					else
						return -1;
				default:
					Com_Printf ("NET_GetPacket: %s from %s\n", LOG_NET, strerror(e->ee_errno), NET_AdrToString (net_from));
					continue;
			}
		}

		//errno = err;
		//Com_Printf ("NET_GetPacket: %s\n", LOG_NET, NET_ErrorString());
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
		Com_Printf ("NET_SendPacket to %s: ERROR: %s\n", LOG_NET, NET_AdrToString(to), NET_ErrorString());
		return 0;
	}

	net_packets_out++;
	net_total_out += ret;
	return 1;
}

//=============================================================================

/*
====================
NET_Init
====================
*/
void NET_Init (void)
{
	NET_Common_Init ();
	net_no_recverr = Cvar_Get ("net_no_recverr", "0", 0);
}


/*
====================
NET_Socket
====================
*/
int NET_IPSocket (char *net_interface, int port)
{
	int newsocket;
	struct sockaddr_in address;
	qboolean _true = true;
	int	i = 1;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		Com_Printf ("UDP_OpenSocket: Couldn't make socket: %s\n", LOG_NET, NET_ErrorString());
		return 0;
	}

	if (newsocket >= FD_SETSIZE)
		Com_Error (ERR_FATAL, "NET_IPSocket: socket is higher than FD_SETSIZE");

	// make it non-blocking
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
	{
		Com_Printf ("UDP_OpenSocket: Couldn't make non-blocking: %s\n", LOG_NET, NET_ErrorString());
		return 0;
	}

	// make it broadcast capable
	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) == -1)
	{
		Com_Printf ("UDP_OpenSocket: Couldn't set SO_BROADCAST: %s\n", LOG_NET, NET_ErrorString());
		return 0;
	}

	// r1: accept icmp unreachables for quick disconnects
	if (!net_no_recverr->intvalue)
	{
		if (setsockopt (newsocket, IPPROTO_IP, IP_RECVERR, (char *)&i, sizeof(i)) == -1)
		{
			Com_Printf ("UDP_OpenSocket: Couldn't set IP_RECVERR: %s\n", LOG_NET, NET_ErrorString());
		}
	}

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
		close (newsocket);
		Com_Printf ("UDP_OpenSocket: Couldn't bind to UDP port %d: %s\n", LOG_NET, port, NET_ErrorString());
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


