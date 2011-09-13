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

#include "qcommon.h"

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16	qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct evon/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.


The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.


If there is no information that needs to be transfered on a given frame,
such as during the connection stage while waiting for the client to load,
then a packet only needs to be delivered if there is something in the
unacknowledged reliable
*/

cvar_t		*showpackets;
cvar_t		*showdrop;
cvar_t		*qport;
cvar_t		*net_maxmsglen;

netadr_t	net_from;
sizebuf_t	net_message;
byte		net_message_buffer[MAX_MSGLEN];

/*
===============
Netchan_Init

===============
*/
void Netchan_Init (void)
{
	int		port;

	// pick a port value that should be nice and random
	port = (int)(random() * 0xFFFF);

	showpackets = Cvar_Get ("showpackets", "0", 0);
	showdrop = Cvar_Get ("showdrop", "0", 0);

	//-1 = random, 0 = none, other = user set
	qport = Cvar_Get ("qport", "-1", 0);

	net_maxmsglen = Cvar_Get ("net_maxmsglen", "1390", 0);
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (int net_socket, netadr_t *adr, int length, const byte *data)
{
	sizebuf_t	send;
	byte		send_buf[MAX_MSGLEN];
	//byte		send_buf[MAX_MSGLEN*4+32];

// write the packet header
	SZ_Init (&send, send_buf, sizeof(send_buf));
	
	SZ_WriteLong (&send, -1);	// -1 sequence means out of band
	SZ_Write (&send, data, length);

// send the datagram
	NET_SendPacket (net_socket, send.cursize, send.data, adr);
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint (int net_socket, netadr_t *adr, const char *format, ...)
{
	va_list		argptr;
	static char		string[MAX_MSGLEN - 4];
	
	va_start (argptr, format);
	if (Q_vsnprintf (string, sizeof(string)-1, format,argptr) < 0)
		Com_Printf ("WARNING: Netchan_OutOfBandPrint: message overflow.\n", LOG_NET);
	va_end (argptr);

	Netchan_OutOfBand (net_socket, adr, (int)strlen(string), (byte *)string);
}

/*
===============
Netchan_OutOfBandProxy

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBandProxy (int net_socket, netadr_t *adr, int length, const byte *data)
{
	sizebuf_t	send;
	byte		send_buf[MAX_MSGLEN];
	//byte		send_buf[MAX_MSGLEN*4+32];

// write the packet header
	SZ_Init (&send, send_buf, sizeof(send_buf));
	
	SZ_WriteLong (&send, -2);	// -1 sequence means out of band
	SZ_Write (&send, data, length);

// send the datagram
	NET_SendPacket (net_socket, send.cursize, send.data, adr);
}

/*
===============
Netchan_OutOfBandProxyPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandProxyPrint (int net_socket, netadr_t *adr, const char *format, ...)
{
	va_list		argptr;
	static char		string[MAX_MSGLEN - 4];
	
	va_start (argptr, format);
	if (Q_vsnprintf (string, sizeof(string)-1, format,argptr) < 0)
		Com_Printf ("WARNING: Netchan_OutOfBandProxyPrint: message overflow.\n", LOG_NET);
	va_end (argptr);

	Netchan_OutOfBandProxy (net_socket, adr, (int)strlen(string), (byte *)string);
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t *adr, int protocol, int qport, unsigned msglen)
{
	memset (chan, 0, sizeof(*chan));
	
	chan->sock = sock;
	chan->remote_address = *adr;

	if (protocol == PROTOCOL_R1Q2)
	{
		if (msglen)
		{
			if (msglen > MAX_USABLEMSG)
				Com_Error (ERR_DROP, "msglen > MAX_USABLEMSG");
			SZ_Init (&chan->message, chan->message_buf, msglen);
		}
		else
		{
			SZ_Init (&chan->message, chan->message_buf, MAX_USABLEMSG);	//fragmentation allows this
		}
	}
	else
	{
		SZ_Init (&chan->message, chan->message_buf, 1390);			//traditional limit
	}

	chan->qport = qport;
	chan->protocol = protocol;
	chan->last_received = curtime;
	chan->incoming_sequence = 0;
	chan->outgoing_sequence = 1;
	chan->message.allowoverflow = true;
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
int Netchan_Transmit (netchan_t *chan, int length, const byte *data)
{
	sizebuf_t	send;
	byte		send_buf[MAX_MSGLEN];
	qboolean	send_reliable;
	uint32		w1, w2;
	unsigned	i;

	// check for message overflow (this is only for client now ?)
	if (chan->message.overflowed)
	{
		//chan->fatal_error = true;
		Com_Printf ("%s:Outgoing message overflow (o:%d, %d bytes)\n", LOG_NET
			, NET_AdrToString (&chan->remote_address), chan->message.overflowed, chan->message.cursize);
		return -2;
	}

	send_reliable =
		(
			(	chan->incoming_acknowledged > chan->last_reliable_sequence &&
				chan->incoming_reliable_acknowledged != chan->reliable_sequence
			)
			||
			(
				!chan->reliable_length && chan->message.cursize
			)
		);

	if (!chan->reliable_length && chan->message.cursize)
	{
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
	}


// write the packet header
	if (chan->protocol == PROTOCOL_R1Q2)
		SZ_Init (&send, send_buf, sizeof(send_buf));
	else
		SZ_Init (&send, send_buf, 1400);

	w1 = ( chan->outgoing_sequence & ~(1<<31) ) | (send_reliable<<31);
	w2 = ( chan->incoming_sequence & ~(1<<31) ) | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;
	chan->last_sent = curtime;

	SZ_WriteLong (&send, w1);
	SZ_WriteLong (&send, w2);

	// send the qport if we are a client
	if (chan->sock == NS_CLIENT)
	{
		if (chan->protocol != PROTOCOL_R1Q2)
			SZ_WriteShort (&send, chan->qport);
		else if (chan->qport)
			SZ_WriteByte (&send, chan->qport);
	}

// copy the reliable message to the packet first
	if (send_reliable)
	{
		if (chan->reliable_length)
			SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		else
			Com_DPrintf ("Netchan_Transmit: send_reliable with empty buffer to %s!\n", NET_AdrToString (&chan->remote_address));
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}
	
// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length)
	{
		if (length)
		{
			SZ_Write (&send, data, length);
		}
	}
	else
	{
		//Com_Printf ("Netchan_Transmit: dumped unreliable to %s (max %d - cur %d >= un %d (r=%d))\n", LOG_NET, NET_AdrToString(&chan->remote_address), send.maxsize, send.cursize, length, chan->reliable_length);
		Com_Error (ERR_DROP, "Netchan_Transmit: reliable %d + unreliable %d > maxsize %d (this should not happen!)", send.cursize, length, send.maxsize);
	}

// send the datagram
	for (i = 0; i <= chan->packetdup; i++)
	{
		if (NET_SendPacket (chan->sock, send.cursize, send_buf, &chan->remote_address) == -1)
			return -1;
	}

	if (showpackets->intvalue)
	{
		if (send_reliable)
			Com_Printf ("send %4i : s=%i reliable=%i ack=%i rack=%i\n", LOG_NET
				, send.cursize
				, chan->outgoing_sequence - 1
				, chan->reliable_sequence
				, chan->incoming_sequence
				, chan->incoming_reliable_sequence);
		else
			Com_Printf ("send %4i : s=%i ack=%i rack=%i\n", LOG_NET
				, send.cursize
				, chan->outgoing_sequence - 1
				, chan->incoming_sequence
				, chan->incoming_reliable_sequence);
	}

	return 0;
}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qboolean Netchan_Process (netchan_t *chan, sizebuf_t *msg)
{
	uint32		sequence, sequence_ack;
	int32		reliable_ack;
	uint32		reliable_message;

	// get sequence numbers		
	MSG_BeginReading (msg);

	sequence = MSG_ReadLong (msg);
	sequence_ack = MSG_ReadLong (msg);

	// read the qport if we are a server
	if (chan->sock == NS_SERVER)
	{
		//suck up 2 bytes for original and old r1q2
		if (chan->protocol != PROTOCOL_R1Q2 || chan->qport > 0xFF)
			MSG_ReadShort (msg);
		else if (chan->qport)
			MSG_ReadByte (msg);
	}

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	chan->got_reliable = reliable_message;

	sequence &= ~(1<<31);
	sequence_ack &= ~(1<<31);	

	if (showpackets->intvalue)
	{
		if (reliable_message)
			Com_Printf ("recv %4i : s=%i reliable=%i ack=%i rack=%i\n", LOG_NET
				, msg->cursize
				, sequence
				, chan->incoming_reliable_sequence ^ 1
				, sequence_ack
				, reliable_ack);
		else
			Com_Printf ("recv %4i : s=%i ack=%i rack=%i\n", LOG_NET
				, msg->cursize
				, sequence
				, sequence_ack
				, reliable_ack);
	}

//
// discard stale or duplicated packets
//

	if (sequence <= chan->incoming_sequence)
	{
		if (showdrop->intvalue)
			Com_Printf ("%s:Out of order packet %i at %i\n", LOG_NET
				, NET_AdrToString (&chan->remote_address)
				,  sequence
				, chan->incoming_sequence);
		return false;
	}

//
// dropped packets don't keep the message from being used
//
	chan->dropped = sequence - (chan->incoming_sequence+1);

	if (chan->dropped > 0 && showdrop->intvalue)
	{
		Com_Printf ("%s:Dropped %i packets at %i\n", LOG_NET
		, NET_AdrToString (&chan->remote_address)
		, chan->dropped
		, sequence);
	}

	//r1: pl stats
	chan->total_received++;
	chan->total_dropped += chan->dropped;

//
// if the current outgoing reliable message has been acknowledged
// clear the buffer to make way for the next
//
	if (reliable_ack == chan->reliable_sequence)
		chan->reliable_length = 0;	// it has been received
	
//
// if this message contains a reliable message, bump incoming_reliable_sequence 
//
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
	{
		chan->incoming_reliable_sequence ^= 1;
	}

//
// the message can now be read from the current message pointer
//

	chan->last_received = curtime;

	return true;
}
