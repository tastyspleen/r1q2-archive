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
// sv_main.c -- server main program

#include "server.h"

/*
=============================================================================

Com_Printf redirection

=============================================================================
*/

char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect (int sv_redirected, char *outputbuf)
{
	if (sv_redirected == RD_PACKET)
	{
		//NET_SendPacket (NS_SERVER, strlen(outputbuf), outputbuf, net_from);
		Netchan_OutOfBandPrint (NS_SERVER, net_from, "print\n%s", outputbuf);
	}
	/*else if (sv_redirected == RD_CLIENT)
	{
		MSG_BeginWriteByte (&sv_client->netchan.message, svc_print);
		MSG_WriteByte (&sv_client->netchan.message, PRINT_HIGH);
		MSG_WriteString (&sv_client->netchan.message, outputbuf);
	}*/
}


/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

sizebuf_t *MSGQueueAlloc (client_t *cl, int size, byte type)
{
	message_queue_t *msg;

	msg = &cl->messageQueue;

	while (msg->next)
		msg = msg->next;

	msg->next = Z_TagMalloc (sizeof(*msg), TAGMALLOC_MSG_QUEUE);
	msg = msg->next;

	msg->type = type;
	msg->next = NULL;
	msg->queued_frame = sv.framenum;
	msg->data = Z_TagMalloc (size, TAGMALLOC_MSG_QUEUE);

	SZ_Init (&msg->buf, msg->data, size);

	return &msg->buf;
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed if the level passes
=================
*/
void EXPORT SV_ClientPrintf (client_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1400];
	int			msglen;
	sizebuf_t	*dst;

	if (level < cl->messagelevel)
		return;
	
	va_start (argptr,fmt);
	msglen = Q_vsnprintf (string, sizeof(string)-1, fmt, argptr);
	va_end (argptr);
	string[sizeof(string)-1] = 0;

	if (msglen == -1)
	{
		Com_Printf ("SV_ClientPrintf: overflow\n");
		msglen = 1399;
	}

	if (cl->netchan.message.cursize + msglen > cl->netchan.message.maxsize || cl->messageQueue.next)
		dst = MSGQueueAlloc (cl, msglen+3, svc_print);
	else
		dst = &cl->netchan.message;

	MSG_BeginWriteByte (dst, svc_print);
	MSG_WriteByte (dst, level);
	MSG_WriteString (dst, string);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void EXPORT SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[2048];
	client_t	*cl;
	int			i;

	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string)-1, fmt,argptr);
	va_end (argptr);
	
	// echo to console
	if (dedicated->value)
	{
		char	copy[1024];
		int		i;
		
		// mask off high bits
		for (i=0 ; i<1023 && string[i] ; i++)
			copy[i] = string[i]&127;
		copy[i] = 0;
		Com_Printf ("%s", copy);
	}

	for (i=0, cl = svs.clients ; i<maxclients->value; i++, cl++)
	{
		if (level < cl->messagelevel)
			continue;
		if (cl->state != cs_spawned)
			continue;
		MSG_BeginWriteByte (&cl->netchan.message, svc_print);
		MSG_WriteByte (&cl->netchan.message, level);
		MSG_WriteString (&cl->netchan.message, string);
	}
}

/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	
	if (!sv.state)
		return;

	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string)-1, fmt, argptr);
	va_end (argptr);

	MSG_BeginWriteByte (&sv.multicast, svc_stufftext);
	MSG_WriteString (&sv.multicast, string);
	SV_Multicast (NULL, MULTICAST_ALL_R);
}


/*
=================
SV_Multicast

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MULTICAST_ALL	same as broadcast (origin can be NULL)
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
void EXPORT SV_Multicast (vec3_t origin, multicast_t to)
{
	client_t	*client;
	byte		*mask;
	int			leafnum, cluster;
	int			j;
	qboolean	reliable;
	int			area1, area2;

	reliable = false;

	if (to != MULTICAST_ALL_R && to != MULTICAST_ALL)
	{
		leafnum = CM_PointLeafnum (origin);
		area1 = CM_LeafArea (leafnum);
	}
	else
	{
		leafnum = 0;	// just to avoid compiler warnings
		area1 = 0;
	}

	// if doing a serverrecord, store everything
	if (svs.demofile)
		SZ_Write (&svs.demo_multicast, sv.multicast.data, sv.multicast.cursize);
	
	switch (to)
	{
	case MULTICAST_ALL_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_ALL:
		leafnum = 0;
		mask = NULL;
		break;

	case MULTICAST_PHS_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_PHS:
		leafnum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafnum);
		mask = CM_ClusterPHS (cluster);
		break;

	case MULTICAST_PVS_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_PVS:
		leafnum = CM_PointLeafnum (origin);
		cluster = CM_LeafCluster (leafnum);
		mask = CM_ClusterPVS (cluster);
		break;

	default:
		mask = NULL;
		Com_Error (ERR_FATAL, "SV_Multicast: bad to:%i", to);
	}

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < maxclients->value; j++, client++)
	{
		if (client->state == cs_free || client->state == cs_zombie)
			continue;
		if (client->state != cs_spawned && !reliable)
			continue;

		if (mask)
		{
			leafnum = CM_PointLeafnum (client->edict->s.origin);
			cluster = CM_LeafCluster (leafnum);
			area2 = CM_LeafArea (leafnum);
			if (!CM_AreasConnected (area1, area2))
				continue;
			if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)) ) ) )
				continue;
		}

		if (reliable)
			SZ_Write (&client->netchan.message, sv.multicast.data, sv.multicast.cursize);
		else
			SZ_Write (&client->datagram, sv.multicast.data, sv.multicast.cursize);
	}

	SZ_Clear (&sv.multicast);
}


/*  
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

If cahnnel & 8, the sound will be sent to everyone, not just
things in the PHS.

FIXME: if entity isn't in PHS, they must be forced to be sent or
have the origin explicitly sent.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Timeofs can range from 0.0 to 0.1 to cause sounds to be started
later in the frame than they normally would.

If origin is NULL, the origin is determined from the entity origin
or the midpoint of the entity box for bmodels.
==================
*/  
void EXPORT SV_StartSound (vec3_t origin, edict_t *entity, int channel,
					int soundindex, float volume,
					float attenuation, float timeofs)
{       
	int			sendchan;
    int			flags;
    int			i, j;
	int			ent;
	vec3_t		origin_v;
	qboolean	use_phs;
	client_t	*client;
	sizebuf_t	*to;
	qboolean	force_pos= false;

	if (volume < 0 || volume > 1.0)
		Com_Error (ERR_FATAL, "SV_StartSound: volume = %f", volume);

	if (attenuation < 0 || attenuation > 4)
		Com_Error (ERR_FATAL, "SV_StartSound: attenuation = %f", attenuation);

//	if (channel < 0 || channel > 15)
//		Com_Error (ERR_FATAL, "SV_StartSound: channel = %i", channel);

	if (timeofs < 0 || timeofs > 0.255)
		Com_Error (ERR_FATAL, "SV_StartSound: timeofs = %f", timeofs);

	ent = NUM_FOR_EDICT(entity);

	if (channel & CHAN_NO_PHS_ADD)	// no PHS flag
	{
		use_phs = false;
		channel &= 7;
	}
	else
		use_phs = true;

	sendchan = (ent<<3) | (channel&7);

	flags = 0;

	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;

	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	// the client doesn't know that bmodels have weird origins
	// the origin can also be explicitly set
	if ( (entity->svflags & SVF_NOCLIENT)
		|| (entity->solid == SOLID_BSP) 
		|| origin) {
		flags |= SND_POS;
		force_pos = true;
		}

	// always send the entity number for channel overrides
	flags |= SND_ENT;

	if (timeofs)
		flags |= SND_OFFSET;

	// use the entity origin unless it is a bmodel or explicitly specified
	if (!origin)
	{
		origin = origin_v;
		if (entity->solid == SOLID_BSP)
		{
			for (i=0 ; i<3 ; i++)
				origin_v[i] = entity->s.origin[i]+0.5*(entity->mins[i]+entity->maxs[i]);
		}
		else
		{
			VectorCopy (entity->s.origin, origin_v);
		}
	}

	if (attenuation == ATTN_NONE)
		use_phs = false;

	for (j = 0, client = svs.clients; j < maxclients->value; j++, client++)
	{
		if (client->state == cs_free || client->state == cs_zombie || (client->state != cs_spawned && !(channel & CHAN_RELIABLE)))
			continue;

		if (use_phs) {
			if (force_pos) {
				flags |= SND_POS;
			} else {
				if (!PF_inPHS (client->edict->s.origin, origin))
					continue;

				if (!PF_inPVS (client->edict->s.origin, origin))
					flags |= SND_POS;
				else
					flags &= ~SND_POS;
			}
		}

		if (channel & CHAN_RELIABLE)
			to = &client->netchan.message;
		else
			to = &client->datagram;
			
		MSG_BeginWriteByte (to, svc_sound);
		MSG_WriteByte (to, flags);
		MSG_WriteByte (to, soundindex);

		if (flags & SND_VOLUME)
			MSG_WriteByte (to, volume*255);

		if (flags & SND_ATTENUATION)
			MSG_WriteByte (to, attenuation*64);

		if (flags & SND_OFFSET)
			MSG_WriteByte (to, timeofs*1000);

		if (flags & SND_ENT)
			MSG_WriteShort (to, sendchan);

		if (flags & SND_POS)
			MSG_WritePos (to, origin);

	}
	// if the sound doesn't attenuate,send it to everyone
	// (global radio chatter, voiceovers, etc)
	/*if (attenuation == ATTN_NONE)
		use_phs = false;

	if (channel & CHAN_RELIABLE)
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS_R);
		else
			SV_Multicast (origin, MULTICAST_ALL_R);
	}
	else
	{
		if (use_phs)
			SV_Multicast (origin, MULTICAST_PHS);
		else
			SV_Multicast (origin, MULTICAST_ALL);
	}*/
}           


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/



/*
=======================
SV_SendClientDatagram
=======================
*/
qboolean SV_SendClientDatagram (client_t *client)
{
//	qboolean	temp;
	byte		msg_buf[MAX_MSGLEN];
	sizebuf_t	msg;
	int			ret;

	SV_BuildClientFrame (client);

	SZ_Init (&msg, msg_buf, sizeof(msg_buf));
	msg.allowoverflow = true;

	// send over all the relevant entity_state_t
	// and the player_state_t
	if (!client->nodata)
		SV_WriteFrameToClient (client, &msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	// it is necessary for this to be after the WriteEntities
	// so that entity references will be current
	if (client->datagram.overflowed)
		Com_Printf ("WARNING: datagram overflowed for %s\n", client->name);
	else
		SZ_Write (&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear (&client->datagram);

	//temp = client->netchan.message.overflowed;

	if (msg.overflowed)
	{	// must have room left for the packet header
		Com_DPrintf ("WARNING: msg overflowed for %s\n", client->name);
		SZ_Clear (&msg);
		//client->netchan.message.overflowed = true;
	} else {

		//r1ch: fill in any spare room with msg queue
		while (client->messageQueue.next)
		{
			message_queue_t *msgq;
			int remainingSpace;
			int remainingMsg;

			msgq = client->messageQueue.next;

			remainingSpace = MAX_USABLEMSG - client->netchan.message.cursize - msg.cursize;

			if (remainingSpace < 200)
				break;

			remainingMsg = msgq->buf.cursize;

			if (remainingMsg < remainingSpace)
			{
				SZ_Write (&client->netchan.message, msgq->data, msgq->buf.cursize);

				client->messageQueue.next = msgq->next;
				Z_Free (msgq->data);
				Z_Free (msgq);
			}
			else
			{
				if (client->protocol == ENHANCED_PROTOCOL_VERSION)
				{
					byte zBuff[4096];
					int zLen;

					zLen = ZLibCompressChunk (msgq->data, msgq->buf.cursize, zBuff, sizeof(zBuff), Z_DEFAULT_COMPRESSION, -15);

					if (zLen + 5 >= remainingSpace || !zLen)
						break;

					MSG_BeginWriteByte (&client->netchan.message, svc_zpacket);
					MSG_WriteShort (&client->netchan.message, zLen);
					MSG_WriteShort (&client->netchan.message, msgq->buf.cursize);
					SZ_Write (&client->netchan.message, zBuff, zLen);

					client->messageQueue.next = msgq->next;
					Z_Free (msgq->data);
					Z_Free (msgq);
				}
				else
				{
					break;
				}
			}
		}

		if (client->zlevel && msg.cursize > client->zlevel)
		{
			byte message[4096], buffer[4096], *p;
			int compressedLen;

			p = message;
			*p = svc_zpacket;
			p++;
			compressedLen = ZLibCompressChunk (msg.data, msg.cursize, buffer, sizeof(buffer), 9, -15);
		
			*(short *)p = compressedLen;
			p += sizeof(short);

			*(short *)p = msg.cursize;
			p += sizeof(short);

			memcpy (p, buffer, compressedLen);
			p += compressedLen;
			SZ_Clear (&msg);
			SZ_Write (&msg, message, p - message);
		}

		// send the datagram
		ret = Netchan_Transmit (&client->netchan, msg.cursize, msg.data);
		if (ret == -1)
		{
			SV_KickClient (client, "connection reset by peer", NULL);
			return false;
		}
		else if (ret == -2)
		{
			SV_KickClient (client, "outgoing message overflow", NULL);
			return false;
		}
	}
	//client->netchan.message.overflowed = temp;

	// record the size for rate estimation
	client->message_size[sv.framenum % RATE_MESSAGES] = msg.cursize;

	return true;
}


/*
==================
SV_DemoCompleted
==================
*/
void SV_DemoCompleted (void)
{
	if (sv.demofile)
	{
		fclose (sv.demofile);
		sv.demofile = NULL;
	}
	SV_Nextserver ();
}


/*
=======================
SV_RateDrop

Returns true if the client is over its current
bandwidth estimation and should not be sent another packet
=======================
*/
qboolean SV_RateDrop (client_t *c)
{
	int		total;
	int		i;

	// never drop over the loopback
	if (c->netchan.remote_address.type == NA_LOOPBACK)
		return false;

	total = 0;

	for (i = 0 ; i < RATE_MESSAGES ; i++)
	{
		total += c->message_size[i];
	}

	if (total > c->rate)
	{
		c->surpressCount++;
		c->message_size[sv.framenum % RATE_MESSAGES] = 0;
		return true;
	}

	return false;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int			i;
	client_t	*c;
	int			msglen;
	byte		msgbuf[MAX_MSGLEN];
	int			r;

	msglen = 0;

	// read the next demo message if needed
	if (sv.state == ss_demo && sv.demofile)
	{
		if (sv_paused->value)
			msglen = 0;
		else
		{
			// get the next message
			r = fread (&msglen, 4, 1, sv.demofile);
			if (r != 1)
			{
				SV_DemoCompleted ();
				return;
			}
			msglen = LittleLong (msglen);
			if (msglen == -1)
			{
				SV_DemoCompleted ();
				return;
			}
			if (msglen > MAX_MSGLEN)
				Com_Error (ERR_DROP, "SV_SendClientMessages: msglen > MAX_MSGLEN");
			r = fread (msgbuf, msglen, 1, sv.demofile);
			if (r != 1)
			{
				SV_DemoCompleted ();
				return;
			}
		}
	}

	// send a message to each connected client
	for (i=0, c = svs.clients ; i<maxclients->value; i++, c++)
	{
		if (!c->state)
			continue;

		// if the reliable message overflowed,
		// drop the client
		if (c->netchan.message.overflowed)
		{
#ifndef NO_ZLIB
			if (c->netchan.protocol == ENHANCED_PROTOCOL_VERSION)
			{
				byte message[4096], buffer[4096], *p;
				int compressedLen;

				if (c->netchan.message.cursize >= 4096)
				{
					if (*c->name)
					{
						if (c->state == cs_spawned) {
							SV_BroadcastPrintf (PRINT_HIGH, "%s's buffer got too big (%d)\n", c->name, c->netchan.message.cursize);
						} else {
							SV_ClientPrintf (c, PRINT_HIGH, "%s's buffer got too big (%d)\n", c->name, c->netchan.message.cursize);
						}
					}

					SZ_Clear (&c->netchan.message);
					SZ_Clear (&c->datagram);

					SV_DropClient (c);
				}
				else
				{
					p = message;
					*p = svc_zpacket;
					p++;
					compressedLen = ZLibCompressChunk (c->netchan.message.data, c->netchan.message.cursize, buffer, sizeof(buffer), Z_DEFAULT_COMPRESSION, -15);
					if (compressedLen > MAX_USABLEMSG || !compressedLen)
					{
						//r1: stop overflow spam from unconnected clients
						if (*c->name)
						{
							if (c->state == cs_spawned) {
								SV_BroadcastPrintf (PRINT_HIGH, "%s still overflowed even after zPacket (%d->%d(>%d))\n", c->name, c->netchan.message.cursize, compressedLen, c->netchan.message.maxsize);
							} else {
								SV_ClientPrintf (c, PRINT_HIGH, "%s still overflowed even after zPacket (%d->%d(>%d))\n", c->name, c->netchan.message.cursize, compressedLen, c->netchan.message.maxsize);
							}
						}

						SZ_Clear (&c->netchan.message);
						SZ_Clear (&c->datagram);

						SV_DropClient (c);
					}
					else
					{
						*(short *)p = compressedLen;
						p += sizeof(short);
						*(short *)p = c->netchan.message.cursize;
						p += sizeof(short);
						memcpy (p, buffer, compressedLen);
						p += compressedLen;
						SZ_Clear (&c->netchan.message);
						SZ_Write (&c->netchan.message, message, p - message);
					}
				}
			}
			else
#endif
			{
				Com_Printf ("WARNING: %s overflowed buffer by %d bytes\n", c->name, c->netchan.message.cursize);
				SZ_Clear (&c->netchan.message);
				SZ_Clear (&c->datagram);

				//r1: stop overflow spam from unconnected clients
				if (*c->name)
				{
					if (c->state == cs_spawned)
					{
						SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
					} else {
						SV_ClientPrintf (c, PRINT_HIGH, "%s overflowed\n", c->name);
					}
				}
				SV_DropClient (c);
			}
		}
		/*
		else
		{
			if (c->netchan.message.cursize > MAX_USABLEMSG)
				c->netchan.message.overflowed = true;

			if (c->netchan.message.overflowed)
			{
					SZ_Clear (&c->netchan.message);
					SZ_Clear (&c->datagram);

					//r1: stop overflow spam from unconnected clients
					if (*c->name)
					{
						if (c->state == cs_spawned) {
							SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
						} else {
							SV_ClientPrintf (c, PRINT_HIGH, "%s overflowed\n", c->name);
						}
					}
					SV_DropClient (c);
				}
			}
		}*/

		if (sv.state == ss_cinematic 
			|| sv.state == ss_demo 
			|| sv.state == ss_pic
			)
			Netchan_Transmit (&c->netchan, msglen, msgbuf);
		else if (c->state == cs_spawned)
		{
			// don't overrun bandwidth
			if (SV_RateDrop (c))
				continue;

			SV_SendClientDatagram (c);
		}
		else
		{
			// just update reliable	if needed
			if (c->netchan.message.cursize	|| curtime - c->netchan.last_sent > 1000 )
				Netchan_Transmit (&c->netchan, 0, NULL);
		}
	}
}

