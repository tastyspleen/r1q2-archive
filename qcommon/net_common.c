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
/*#define DO(src,dest)	\
	copy[0] = s[src];	\
	copy[1] = s[src + 1];	\
	sscanf (copy, "%x", &val);	\
	((struct sockaddr_ipx *)sadr)->dest = val*/


#define SockadrToNetadr(s,a) \
	a->type = NA_IP; \
	*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr; \
	a->port = ((struct sockaddr_in *)s)->sin_port; \

qboolean	NET_StringToSockaddr (char *s, struct sockaddr *sadr)
{
	int	isip = 0;
	char *p;
	struct hostent	*h;
	char	*colon;
//	int		val;
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

	//r1: CHECK THE GODDAMN BUFFER SIZE... sigh yet another overflow.
	strncpy (copy, s, sizeof(copy)-1);

	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++) {
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
			break;
		}
	}
	
	if (isip)
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if (! (h = gethostbyname(copy)) )
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}

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
qboolean	NET_StringToAdr (char *s, netadr_t *a)
{
	struct sockaddr sadr;

	if (!strcmp (s, "localhost"))
	{
		memset (a, 0, sizeof(*a));

		//r1: should need some kind of ip data to prevent comparisons with empty ips?
		a->ip[0] = 127;
		a->ip[3] = 1;
		a->type = NA_LOOPBACK;
		return true;
	}

	if (!NET_StringToSockaddr (s, &sadr))
		return false;
	
	SockadrToNetadr (&sadr, a);

	return true;
}


void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof(*s));

	if (a->type == NA_IP)
	{
		s->sin_family = AF_INET;

		*(int *)&s->sin_addr = *(int *)&a->ip;
		s->sin_port = a->port;
	}
	else if (a->type == NA_BROADCAST)
	{
		s->sin_family = AF_INET;

		s->sin_port = a->port;
		*(int *)&s->sin_addr = -1;
	}
}

char	*NET_AdrToString (netadr_t *a)
{
	static	char	s[64];
	
	Com_sprintf (s, sizeof(s), "%i.%i.%i.%i:%i", a->ip[0], a->ip[1], a->ip[2], a->ip[3], ntohs(a->port));

	return s;
}

char	*NET_BaseAdrToString (netadr_t *a)
{
	static	char	s[64];
	
	Com_sprintf (s, sizeof(s), "%i.%i.%i.%i", a->ip[0], a->ip[1], a->ip[2], a->ip[3]);

	return s;
}


#ifndef DEDICATED_ONLY

qboolean	NET_GetLoopPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy (net_message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message->cursize = loop->msgs[i].datalen;
	memset (net_from, 0, sizeof(*net_from));
	net_from->type = NA_LOOPBACK;
	net_from->ip[0] = 127;
	net_from->ip[3] = 1;
	return true;

}


void NET_SendLoopPacket (netsrc_t sock, int length, void *data)
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock^1];

	i = loop->send & (MAX_LOOPBACK-1);
	loop->send++;

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

#endif
