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

/*
=============================================================================

Encode a client frame onto the network channel

=============================================================================
*/

#if 0

// because there can be a lot of projectiles, there is a special
// network protocol for them
#define	MAX_PROJECTILES		64
edict_t	*projectiles[MAX_PROJECTILES];
int		numprojs;
cvar_t  *sv_projectiles;

qboolean SV_AddProjectileUpdate (edict_t *ent)
{
	if (!sv_projectiles)
		sv_projectiles = Cvar_Get("sv_projectiles", "1", 0);

	if (!sv_projectiles->value)
		return false;

	if (!(ent->svflags & SVF_PROJECTILE))
		return false;
	if (numprojs == MAX_PROJECTILES)
		return true;

	projectiles[numprojs++] = ent;
	return true;
}

void SV_EmitProjectileUpdate (sizebuf_t *msg)
{
	byte	bits[16];	// [modelindex] [48 bits] xyz p y 12 12 12 8 8 [entitynum] [e2]
	int		n, i;
	edict_t	*ent;
	int		x, y, z, p, yaw;
	int len;

	if (!numprojs)
		return;

	MSG_WriteByte (msg, numprojs);

	for (n=0 ; n<numprojs ; n++)
	{
		ent = projectiles[n];
		x = (int)(ent->s.origin[0]+4096)>>1;
		y = (int)(ent->s.origin[1]+4096)>>1;
		z = (int)(ent->s.origin[2]+4096)>>1;
		p = (int)(256*ent->s.angles[0]/360)&255;
		yaw = (int)(256*ent->s.angles[1]/360)&255;

		len = 0;
		bits[len++] = x;
		bits[len++] = (x>>8) | (y<<4);
		bits[len++] = (y>>4);
		bits[len++] = z;
		bits[len++] = (z>>8);
		if (ent->s.effects & EF_BLASTER)
			bits[len-1] |= 64;

		if (ent->s.old_origin[0] != ent->s.origin[0] ||
			ent->s.old_origin[1] != ent->s.origin[1] ||
			ent->s.old_origin[2] != ent->s.origin[2]) {
			bits[len-1] |= 128;
			x = (int)(ent->s.old_origin[0]+4096)>>1;
			y = (int)(ent->s.old_origin[1]+4096)>>1;
			z = (int)(ent->s.old_origin[2]+4096)>>1;
			bits[len++] = x;
			bits[len++] = (x>>8) | (y<<4);
			bits[len++] = (y>>4);
			bits[len++] = z;
			bits[len++] = (z>>8);
		}

		bits[len++] = p;
		bits[len++] = yaw;
		bits[len++] = ent->s.modelindex;

		bits[len++] = (ent->s.number & 0x7f);
		if (ent->s.number > 255) {
			bits[len-1] |= 128;
			bits[len++] = (ent->s.number >> 7);
		}

		for (i=0 ; i<len ; i++)
			MSG_WriteByte (msg, bits[i]);
	}
}
#endif

/*
#define PACKER_BUFFER_SIZE	256
#define BITS_PER_WORD		32

typedef struct packer_s
{
	int		next_bit_to_write;
	byte	buffer[MAX_EDICTS * sizeof(uint16)];	//worst case
} packer_t;

void BP_pack (packer_t *packer, uint32 value, uint32 num_bits_to_pack)
{
	int		byte_index;
	int		bit_index;
	int		empty_space_this_byte;
	byte	*dest;

	Q_assert(num_bits_to_pack <= 32);
	Q_assert((value & ((1 << num_bits_to_pack) - 1)) == value);

	// Scoot the value bits up to the top of the word; this makes
	// them easier to work with.

	value <<= (BITS_PER_WORD - num_bits_to_pack);

	// First we do the hard part: pack bits into the first u8,
	// which may already have bits in it.

	byte_index = (packer->next_bit_to_write / 8);
	bit_index = (packer->next_bit_to_write % 8);
	empty_space_this_byte = (8 - bit_index) & 0x7;

	// Update next_bit_to_write for the next call; we don't need 
	// the old value any more.

	packer->next_bit_to_write += num_bits_to_pack;

	dest = packer->buffer + byte_index;

	if (empty_space_this_byte)
	{
		int	fill_bits;
		int to_copy = empty_space_this_byte;
		int align = 0;

		if (to_copy > num_bits_to_pack)
		{
			// We don't have enough bits to fill up this u8.
			align = to_copy - num_bits_to_pack;
			to_copy = num_bits_to_pack;
		}

		fill_bits = value >> (BITS_PER_WORD - empty_space_this_byte);
		*dest |= fill_bits;

		num_bits_to_pack -= to_copy;
		dest++;
		value <<= to_copy;
	}

	// Now we do the fast and easy part for what is hopefully
	// the bulk of the data.

	while (value)
	{
		*dest++ = value >> (BITS_PER_WORD - 8);
		value <<= 8;
	}
}

int BP_get_length (packer_t *packer)
{
	int len_in_bytes = (packer->next_bit_to_write + 7) / 8;
	Q_assert(len_in_bytes <= PACKER_BUFFER_SIZE);
	return len_in_bytes;
}*/

/*
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message.
=============
*/
static void SV_EmitPacketEntities (const client_t *cl, const client_frame_t /*@null@*/*from, const client_frame_t *to, sizebuf_t *msg)
{
	const entity_state_t	*oldent;
	const entity_state_t	*newent;

//	int				removed[MAX_EDICTS];
//	int				removedindex;
	int				oldindex, newindex;
	int				oldnum, newnum;
	int				from_num_entities;
	int				bits;

#if 0
	if (numprojs)
		MSG_BeginWriting (svc_packetentities2);
	else
#endif

//	removedindex = 0;

	//r1: pointless waste of byte since this is already inside an svc_frame
	if (cl->protocol != ENHANCED_PROTOCOL_VERSION)
		MSG_BeginWriting (svc_packetentities);

	if (!from)
		from_num_entities = 0;
	else
		from_num_entities = from->num_entities;

	newindex = 0;
	oldindex = 0;

	newent = NULL;
	oldent = NULL;

	while (newindex < to->num_entities || oldindex < from_num_entities)
	{
		//r1: anti-packet overflow
		//note, worst case delta will generate 47 bytes of output. this should be extremely rare so we use 40.
		if (sv_packetentities_hack->intvalue && MSG_GetLength() + msg->cursize >= (msg->maxsize - 40))
			break;

		if (newindex >= to->num_entities)
			newnum = 9999;
		else
		{
			newent = &svs.client_entities[(to->first_entity+newindex)%svs.num_client_entities];
			newnum = newent->number;
		}

		if (oldindex >= from_num_entities)
			oldnum = 9999;
		else
		{
			//Com_Printf ("server: its in old entities!\n");
			oldent = &svs.client_entities[(from->first_entity+oldindex)%svs.num_client_entities];
			oldnum = oldent->number;
		}

		//don't send player to self
		//FIXME: need to do this for s.event on self to work etc...
		/*if (newnum == cl->edict->s.number) {
			//Com_Printf ("didn't send edict %d\n", newnum);
			newindex++;
			oldindex++;
			continue;
		}*/

		if (newnum == oldnum)
		{	// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emited if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping

			MSG_WriteDeltaEntity (oldent, newent, false, newent->number <= maxclients->intvalue, cl->protocol);

			oldindex++;
			newindex++;
			continue;
		}
	
		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (&cl->lastlines[newnum], newent, true, true, cl->protocol);

			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			//Com_Printf ("server: remove!!!\n");
			/*if (cl->protocol == ENHANCED_PROTOCOL_VERSION)
			{
				removed[removedindex++] = oldnum;
			}
			else*/
			{
				bits = U_REMOVE;
				if (oldnum >= 256)
					bits |= U_NUMBER16 | U_MOREBITS1;

				MSG_WriteByte (bits&255 );
				if (bits & 0x0000ff00)
					MSG_WriteByte ((bits>>8)&255 );

				if (bits & U_NUMBER16)
					MSG_WriteShort (oldnum);
				else
					MSG_WriteByte (oldnum);
			}

			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (0);	// end of packetentities

	//pack all the removed entities as efficiently as possible using a packer
	//reference: http://number-none.com/product/Packing%20Integers/
#if 0
	if (cl->protocol == ENHANCED_PROTOCOL_VERSION)
	{
 		if (removedindex)
		{
			int		i;
			int		maxEntNum;

			maxEntNum = svs.client_entities[(from->first_entity + from->num_entities-1) %svs.num_client_entities ].number + 1;

			if (maxEntNum < 256)
			{
				for (i = 0; i < removedindex; i++)
					MSG_WriteByte (removed[i]);

				MSG_WriteByte (0);
			}
			else
			{
				packer_t	bitpacker;
				int			byteCount;

				memset (&bitpacker, 0, sizeof(bitpacker));

				for (i = 0; i < removedindex; i++)
				{
					BP_pack (&bitpacker, removed[i], 10);
				}

				byteCount = BP_get_length(&bitpacker);

				MSG_WriteByte (byteCount);
				MSG_Write (bitpacker.buffer, byteCount);
			}

			/*for (i = 0; i < removedindex; i++)
			{
				//check for overflow
				if (i && i % flushNum == 0)
				{
					MSG_WriteLong (accumulator);
					accumulator = 0;
				}
				accumulator = (maxEntNum * accumulator) + removed[i];
			}

			for (i = 0; i < removedindex; i++)
			{
				quotient = accumulator / maxEntNum;
				remainder = accumulator % maxEntNum;

				accumulator = quotient;
				Com_Printf ("packed: %d\n", LOG_GENERAL, remainder);
			}*/
		}
		else
		{
			if (from)
				MSG_WriteByte (0);
		}
	}
#endif

	MSG_EndWriting (msg);
#if 0
	if (numprojs)
		SV_EmitProjectileUpdate(msg);
#endif
}

#define Vec_RangeCap(x,minv,maxv) \
do { \
	if ((x)[0] > (maxv)) (x)[0] = (maxv); else if ((x)[0] < (minv)) x[0] = (minv); \
	if ((x)[1] > (maxv)) (x)[1] = (maxv); else if ((x)[1] < (minv)) x[1] = (minv); \
	if ((x)[2] > (maxv)) (x)[2] = (maxv); else if ((x)[2] < (minv)) x[2] = (minv); \
} while(0)

//performs comparison on encoded byte differences - pointless sending 0.00 -> 0.01 if both end up as 0 on net.
#define Vec_ByteCompare(v1,v2) \
	((int)(v1[0]*4)==(int)(v2[0]*4) && \
	(int)(v1[1]*4)==(int)(v2[1]*4) && \
	(int)(v1[2]*4) == (int)(v2[2]*4))

/*
=============
SV_WritePlayerstateToClient

=============
*/
static int SV_WritePlayerstateToClient (const client_frame_t /*@null@*/*from, client_frame_t *to, sizebuf_t *msg, const client_t *client)
{
	int							i;
	int							pflags;
	static player_state_new		null_playerstate;
	int							statbits;
	int							extraflags;
	qboolean					enhanced;
	player_state_new			*ps;
	const player_state_new		*ops;

	ps = &to->ps;

	extraflags = 0;
	enhanced = (client->protocol == ENHANCED_PROTOCOL_VERSION);

	if (!from)
		ops = &null_playerstate;
	else
		ops = &from->ps;

	//r1: cap to byte range for these
	Vec_RangeCap (ps->viewoffset, -32, 31.75);
	Vec_RangeCap (ps->kick_angles, -32, 31.75);

	//r1: fix signed char range errors
	Vec_RangeCap (ps->gunoffset, -32, 31.75);
	Vec_RangeCap (ps->gunangles, -32, 31.75);

	//and these are written as bytes too
	if (ps->blend[0] > 1)
		ps->blend[0] = 1;

	if (ps->blend[1] > 1)
		ps->blend[1] = 1;

	if (ps->blend[2] > 1)
		ps->blend[2] = 1;

	if (ps->blend[3] > 1)
		ps->blend[3] = 1;

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= PS_M_TYPE;

	//r1
	if (ps->pmove.origin[0] != ops->pmove.origin[0] || ps->pmove.origin[1] != ops->pmove.origin[1])
	{
		if (!enhanced)
			extraflags |= EPS_PMOVE_ORIGIN2;

		pflags |= PS_M_ORIGIN;
	}

	if (ps->pmove.origin[2] != ops->pmove.origin[2])
	{
		if (!enhanced)
			pflags |= PS_M_ORIGIN;
		extraflags |= EPS_PMOVE_ORIGIN2;
	}

	//r1
	if (ps->pmove.velocity[0] != ops->pmove.velocity[0] || ps->pmove.velocity[1] != ops->pmove.velocity[1])
	{
		if (!enhanced)
			extraflags |= EPS_PMOVE_VELOCITY2;

		pflags |= PS_M_VELOCITY;
	}

	if (ps->pmove.velocity[2] != ops->pmove.velocity[2])
	{
		if (!enhanced)
			pflags |= PS_M_VELOCITY;
		extraflags |= EPS_PMOVE_VELOCITY2;
	}

	if (ps->pmove.pm_time != ops->pmove.pm_time)
		pflags |= PS_M_TIME;

	if (ps->pmove.pm_flags != ops->pmove.pm_flags)
		pflags |= PS_M_FLAGS;

	if (ps->pmove.gravity != ops->pmove.gravity)
		pflags |= PS_M_GRAVITY;

	if (ps->pmove.delta_angles[0] != ops->pmove.delta_angles[0]
		|| ps->pmove.delta_angles[1] != ops->pmove.delta_angles[1]
		|| ps->pmove.delta_angles[2] != ops->pmove.delta_angles[2] )
		pflags |= PS_M_DELTA_ANGLES;


	/*if (ps->viewoffset[0] != ops->viewoffset[0]
		|| ps->viewoffset[1] != ops->viewoffset[1]
		|| ps->viewoffset[2] != ops->viewoffset[2] )*/

	if (!Vec_ByteCompare (ps->viewoffset, ops->viewoffset))
		pflags |= PS_VIEWOFFSET;

	if (ps->viewangles[0] != ops->viewangles[0] || ps->viewangles[1] != ops->viewangles[1])
	{
		if (!enhanced)
			extraflags |= EPS_VIEWANGLE2;
		pflags |= PS_VIEWANGLES;
	}
	
	if (ps->viewangles[2] != ops->viewangles[2])
	{
		if (!enhanced)
			pflags |= PS_VIEWANGLES;

		extraflags |= EPS_VIEWANGLE2;
	}

	/*if (ps->kick_angles[0] != ops->kick_angles[0]
		|| ps->kick_angles[1] != ops->kick_angles[1]
		|| ps->kick_angles[2] != ops->kick_angles[2] )*/

	if (!Vec_ByteCompare (ps->kick_angles, ops->kick_angles))
		pflags |= PS_KICKANGLES;

	/*if (ps->blend[0] != ops->blend[0]
		|| ps->blend[1] != ops->blend[1]
		|| ps->blend[2] != ops->blend[2]
		|| ps->blend[3] != ops->blend[3] )*/
	if (!Vec_ByteCompare (ps->blend, ops->blend) || (int)(ps->blend[3]*4) != (int)(ops->blend[3]*4))
		pflags |= PS_BLEND;

	if ((int)ps->fov != (int)ops->fov)
		pflags |= PS_FOV;

	if (ps->rdflags != ops->rdflags)
		pflags |= PS_RDFLAGS;

	if (ps->gunframe != ops->gunframe)
	{
		pflags |= PS_WEAPONFRAME;
		if (!enhanced)
		{
			extraflags |= EPS_GUNANGLES|EPS_GUNOFFSET;
		}
		else
		{
			if (!Vec_ByteCompare (ps->gunangles, ops->gunangles))
				extraflags |= EPS_GUNANGLES;

			if (!Vec_ByteCompare (ps->gunoffset, ops->gunoffset))
				extraflags |= EPS_GUNOFFSET;
		}
	}

	//only possibly send bbox if the client supports it AND the game is supposed to send it
#ifdef ENHANCED_SERVER
	if (client->protocol == ENHANCED_PROTOCOL_VERSION && (!VectorCompare (ps->mins, ops->mins) || !VectorCompare (ps->maxs, ops->maxs)))
		pflags |= PS_BBOX;
#endif

	if (ps->gunindex != ops->gunindex)
		pflags |= PS_WEAPONINDEX;

	//
	// write it
	//

	//r1: pointless waste of byte since this is already inside an svc_frame
	if (!enhanced)
		MSG_BeginWriting (svc_playerinfo);

	MSG_WriteShort (pflags);

	//
	// write the pmove_state_t
	//
	if (pflags & PS_M_TYPE)
		MSG_WriteByte (ps->pmove.pm_type);

	if (pflags & PS_M_ORIGIN)
	{
		MSG_WriteShort (ps->pmove.origin[0]);
		MSG_WriteShort (ps->pmove.origin[1]);
	}

	//r1
	if (extraflags & EPS_PMOVE_ORIGIN2)
		MSG_WriteShort (ps->pmove.origin[2]);
	else if (pflags & PS_M_ORIGIN)
		svs.proto35BytesSaved += 2;

	if (pflags & PS_M_VELOCITY)
	{
		MSG_WriteShort (ps->pmove.velocity[0]);
		MSG_WriteShort (ps->pmove.velocity[1]);
	}

	//r1
	if (extraflags & EPS_PMOVE_VELOCITY2)
		MSG_WriteShort (ps->pmove.velocity[2]);
	else if (pflags & PS_M_VELOCITY)
		svs.proto35BytesSaved += 2;

	if (pflags & PS_M_TIME)
		MSG_WriteByte (ps->pmove.pm_time);

	if (pflags & PS_M_FLAGS)
		MSG_WriteByte (ps->pmove.pm_flags);

	if (pflags & PS_M_GRAVITY)
		MSG_WriteShort (ps->pmove.gravity);

	if (pflags & PS_M_DELTA_ANGLES)
	{
		MSG_WriteShort (ps->pmove.delta_angles[0]);
		MSG_WriteShort (ps->pmove.delta_angles[1]);
		MSG_WriteShort (ps->pmove.delta_angles[2]);
	}

	//
	// write the rest of the player_state_t
	//
	if (pflags & PS_VIEWOFFSET)
	{
		MSG_WriteChar ((int)(ps->viewoffset[0]*4));
		MSG_WriteChar ((int)(ps->viewoffset[1]*4));
		MSG_WriteChar ((int)(ps->viewoffset[2]*4));
	}

	if (pflags & PS_VIEWANGLES)
	{
		MSG_WriteAngle16 (ps->viewangles[0]);
		MSG_WriteAngle16 (ps->viewangles[1]);
	}

	//r1
	if (extraflags & EPS_VIEWANGLE2)
		MSG_WriteAngle16 (ps->viewangles[2]);	//this one rarely changes
	else if (pflags & PS_VIEWANGLES)
		svs.proto35BytesSaved += 2;

	if (pflags & PS_KICKANGLES)
	{
		//r1: fixed, these are read as chars on client!
		MSG_WriteChar ((int)(ps->kick_angles[0]*4));
		MSG_WriteChar ((int)(ps->kick_angles[1]*4));
		MSG_WriteChar ((int)(ps->kick_angles[2]*4));
	}

	if (pflags & PS_WEAPONINDEX)
	{
		MSG_WriteByte (ps->gunindex);
	}

	if (pflags & PS_WEAPONFRAME)
	{
		MSG_WriteByte (ps->gunframe);
	}
	
	//r1
	if (extraflags & EPS_GUNOFFSET)
	{
		MSG_WriteChar ((int)(ps->gunoffset[0]*4));
		MSG_WriteChar ((int)(ps->gunoffset[1]*4));
		MSG_WriteChar ((int)(ps->gunoffset[2]*4));
	}
	else if (pflags & PS_WEAPONFRAME)
	{
		svs.proto35BytesSaved += 3;
	}

	//r1
	if (extraflags & EPS_GUNANGLES)
	{
		MSG_WriteChar ((int)(ps->gunangles[0]*4));
		MSG_WriteChar ((int)(ps->gunangles[1]*4));
		MSG_WriteChar ((int)(ps->gunangles[2]*4));
	}
	else if (pflags & PS_WEAPONFRAME)
	{
		svs.proto35BytesSaved += 3;
	}

	if (pflags & PS_BLEND)
	{
		//r1: fix byte overflow making this lower than it was supposed to be
		MSG_WriteByte ((int)(ps->blend[0]*255));
		MSG_WriteByte ((int)(ps->blend[1]*255));
		MSG_WriteByte ((int)(ps->blend[2]*255));
		MSG_WriteByte ((int)(ps->blend[3]*255));
	}

	if (pflags & PS_FOV)
		MSG_WriteByte ((int)(ps->fov));

	if (pflags & PS_RDFLAGS)
		MSG_WriteByte (ps->rdflags);

	if (pflags & PS_BBOX)
	{
		int j, k;
		int solid;

		i = (int)(ps->maxs[0]/8);
		if (i<1)
			i = 1;
		if (i>31)
			i = 31;

		// z is not symetric
		j = (int)((-ps->mins[2])/8);
		if (j<1)
			j = 1;
		if (j>31)
			j = 31;

		// and z maxs can be negative...
		k = (int)((ps->maxs[2]+32)/8);
		if (k<1)
			k = 1;
		if (k>63)
			k = 63;

		solid = (k<<10) | (j<<5) | i;

		MSG_WriteShort (solid);
	}

	// send stats
	statbits = 0;
	for (i=0 ; i<MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1<<i;

	if (!enhanced || statbits)
		extraflags |= EPS_STATS;

	if (extraflags & EPS_STATS)
	{
		MSG_WriteLong (statbits);
		for (i=0 ; i<MAX_STATS ; i++)
			if (statbits & (1<<i) )
				MSG_WriteShort (ps->stats[i]);
	}
	else
	{
		svs.proto35BytesSaved += 4;
	}

	MSG_EndWriting (msg);

	return extraflags;
}


/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg)
{
	client_frame_t		*frame, *oldframe;
	int					lastframe, framenum;
	int					extraDataIndex, extraflags, serverByteIndex;

	framenum = sv.framenum + sv.randomframe;

//Com_Printf ("%i -> %i\n", client->lastframe, sv.framenum);
	// this is the frame we are creating
	frame = &client->frames[framenum & UPDATE_MASK];

	if (client->lastframe <= 0)
	{	// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (framenum - client->lastframe >= (UPDATE_BACKUP - 2) )
	{	// client hasn't gotten a good message through in a long time
//		Com_Printf ("%s: Delta request from out-of-date packet.\n", client->name);
		oldframe = NULL;
		lastframe = -1;
	}
	else
	{	// we have a valid message to delta from
		oldframe = &client->frames[client->lastframe & UPDATE_MASK];
		lastframe = client->lastframe;
	}

	serverByteIndex = msg->cursize;
	SZ_WriteByte (msg, 0);

	//pack pack pack....
	if (client->protocol == ENHANCED_PROTOCOL_VERSION)
	{
		uint32		offset;
		uint32		encodedFrame;

		//we don't need full 32bits for framenum - 27 gives enough for 155 days on the same map :)
		//could even go lower if we need more bits later
		encodedFrame = framenum & 0x07FFFFFF;

		if (lastframe == -1)
			offset = 31;		//special case
		else
			offset = framenum - lastframe;

		//first 5 bits of framenum = offset for delta
		encodedFrame += (offset << 27);

		SZ_WriteLong (msg, encodedFrame);
		svs.proto35BytesSaved += 4;
	}
	else
	{
		SZ_WriteLong (msg, framenum);
		SZ_WriteLong (msg, lastframe);	// what we are delta'ing from
	}
	extraDataIndex = msg->cursize;
	SZ_WriteByte (msg, 0);
	//SZ_WriteByte (msg, client->surpressCount);	// rate dropped packets
	client->surpressCount = 0;

	// send over the areabits
	SZ_WriteByte (msg, frame->areabytes);
	SZ_Write (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	extraflags = SV_WritePlayerstateToClient (oldframe, frame, msg, client);

	//HOLY CHRIST
	if (client->protocol == ENHANCED_PROTOCOL_VERSION)
	{
		msg->data[serverByteIndex] = svc_frame + ((extraflags & 0xF0) << 1);
		msg->data[extraDataIndex] = client->surpressCount + ((extraflags & 0x0F) << 4);
	}
	else
	{
		msg->data[serverByteIndex] = svc_frame;
		msg->data[extraDataIndex] = client->surpressCount;
	}

	// delta encode the entities
	SV_EmitPacketEntities (client, oldframe, frame, msg);
} 


/*
=============================================================================

Build a client frame structure

=============================================================================
*/

static byte		fatpvs[65536/8];	// 32767 is MAX_MAP_LEAFS

/*
============
SV_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
static void SV_FatPVS (vec3_t org)
{
	int		leafs[64];
	int		i, j, count;
	int		longs;
	byte	*src;
	vec3_t	mins, maxs;

	mins[0] = org[0] - 8;
	maxs[0] = org[0] + 8;
	mins[1] = org[1] - 8;
	maxs[1] = org[1] + 8;
	mins[2] = org[2] - 8;
	maxs[2] = org[2] + 8;

	count = CM_BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = (CM_NumClusters+31)>>5;

	// convert leafs to clusters
	for (i=0 ; i<count ; i++)
		leafs[i] = CM_LeafCluster(leafs[i]);

	memcpy (fatpvs, CM_ClusterPVS(leafs[0]), longs<<2);
	// or in all the other leaf bits
	for (i=1 ; i<count ; i++)
	{
		for (j=0 ; j<i ; j++)
			if (leafs[i] == leafs[j])
				break;
		if (j != i)
			continue;		// already have the cluster we want
		src = CM_ClusterPVS(leafs[i]);
		for (j=0 ; j<longs ; j++)
			((int32 *)fatpvs)[j] |= ((int32 *)src)[j];
	}
}

static qboolean SV_CheckPlayerVisible(vec3_t Angles, vec3_t start, const edict_t *ent, qboolean fullCheck, qboolean predictEnt)
{
	int		i;
	vec3_t	ends[9];
	vec3_t	entOrigin;
	trace_t	trace;
	int		num;

	VectorCopy(ent->s.origin, entOrigin);
	
	if ( predictEnt && ent->client ) {
		for (i = 0; i < 3; i++)

#ifdef ENHANCED_SERVER
			entOrigin[i] += ((struct gclient_old_s *)(ent->client))->ps.pmove.velocity[i] * 0.125f * 0.15f;
#else
			entOrigin[i] += ((struct gclient_old_s *)(ent->client))->ps.pmove.velocity[i] * 0.125f * 0.15f;
#endif

	}

	VectorCopy(entOrigin, ends[0]);

	if ( fullCheck ) {
		vec3_t	right, up;

		for (i = 1; i < 9; i++)
			VectorCopy(entOrigin, ends[i]);

		AngleVectors(Angles, NULL, right, up);

		ends[1][0] += ent->mins[0];
		ends[2][0] += ent->mins[0];
		ends[3][0] += ent->mins[0];
		ends[4][0] += ent->mins[0];
		ends[1][1] += ent->maxs[1];
		ends[2][1] += ent->maxs[1];
		ends[3][1] += ent->mins[1];
		ends[4][1] += ent->mins[1];
		ends[1][2] += ent->maxs[2];
		ends[2][2] += ent->mins[2];
		ends[3][2] += ent->maxs[2];
		ends[4][2] += ent->mins[2];

		ends[5][0] += ent->maxs[0];
		ends[6][0] += ent->maxs[0];
		ends[7][0] += ent->maxs[0];
		ends[8][0] += ent->maxs[0];
		ends[5][1] += ent->maxs[1];
		ends[6][1] += ent->maxs[1];
		ends[7][1] += ent->mins[1];
		ends[8][1] += ent->mins[1];
		ends[5][2] += ent->maxs[2];
		ends[6][2] += ent->mins[2];
		ends[7][2] += ent->maxs[2];
		ends[8][2] += ent->mins[2];

		num = 9;
	}
	else
	{
		num = 1;
	}

	for (i = 0; i < num; i++) {
		trace = SV_Trace(start, NULL, NULL, ends[i], NULL, CONTENTS_SOLID);

		if (trace.fraction == 1)
			return true;
	}

	return false;
}


/*
=============
SV_BuildClientFrame

Decides which entities are going to be visible to the client, and
copies off the playerstat and areabits.
=============
*/
void SV_BuildClientFrame (client_t *client)
{
	int						e, i;
	vec3_t					org;
	edict_t					*ent;
	const edict_t			*clent;
	client_frame_t			*frame;
	entity_state_t			*state;

	int						l;
	int						clientarea, clientcluster;
	int						leafnum;
	int						c_fullsend;
	const byte				*clientphs;
	const byte				*bitvector;

	// *********** NiceAss Start ************
	qboolean	visible;
	vec3_t		start;
	// ***********  NiceAss End  ************

	//union player_state_t	*hax;
	//player_state_new		*ps;

	clent = client->edict;
	if (!clent->client)
		return;		// not in game yet

#if 0
	numprojs = 0; // no projectiles yet
#endif

	// this is the frame we are creating
	frame = &client->frames[(sv.framenum + sv.randomframe) & UPDATE_MASK];

	frame->senttime = svs.realtime; // save it for ping calc later

	//hax = &clent->client->ps;

	
	//	ps = (player_state_new *)&hax->new_ps;
	//else
	//	ps = (player_state_new *)&hax->old_ps;

	// find the client's PVS
	for (i=0 ; i<3 ; i++) {
#ifdef ENHANCED_SERVER
			org[i] = ((struct gclient_new_s *)(clent->client))->ps.pmove.origin[i]*0.125 + ((struct gclient_new_s *)(clent->client))->ps.viewoffset[i];
#else
			org[i] = ((struct gclient_old_s *)(clent->client))->ps.pmove.origin[i]*0.125f + ((struct gclient_old_s *)(clent->client))->ps.viewoffset[i];
#endif
	}

	leafnum = CM_PointLeafnum (org);
	clientarea = CM_LeafArea (leafnum);
	clientcluster = CM_LeafCluster (leafnum);

	// calculate the visible areas
	frame->areabytes = CM_WriteAreaBits (frame->areabits, clientarea);

	// grab the current player_state_t
#ifdef ENHANCED_SERVER
		frame->ps = ((struct gclient_new_s *)(clent->client))->ps;
#else
		memcpy (&frame->ps, &((struct gclient_new_s *)(clent->client))->ps, sizeof(player_state_old));
#endif

	SV_FatPVS (org);
	clientphs = CM_ClusterPHS (clientcluster);

	// build up the list of visible entities
	frame->num_entities = 0;
	frame->first_entity = svs.next_client_entities;

	c_fullsend = 0;

	for (e=1 ; e<ge->num_edicts ; e++)
	{
		ent = EDICT_NUM(e);

		// ignore ents without visible models
		if (ent->svflags & SVF_NOCLIENT)
			continue;

		// ignore ents without visible models unless they have an effect
		if (!ent->s.modelindex && !ent->s.effects && !ent->s.sound
			&& !ent->s.event)
			continue;

		if (!ent->inuse)
		{
			if (sv_gamedebug->intvalue)
				Com_Printf ("GAME WARNING: Entity %d is marked as unused but still contains state and thus may be sent to clients!\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, e);

			if (sv_entity_inuse_hack->intvalue)
				continue;
		}

		// ignore if not touching a PV leaf
		if (ent != clent)
		{
			// check area
			if (!CM_AreasConnected (clientarea, ent->areanum))
			{	// doors can legally straddle two areas, so
				// we may need to check another one
				if (!ent->areanum2
					|| !CM_AreasConnected (clientarea, ent->areanum2))
					continue;		// blocked by a door
			}

			// beams just check one point for PHS
			if (ent->s.renderfx & RF_BEAM)
			{
				l = ent->clusternums[0];
				if ( !(clientphs[l >> 3] & (1 << (l&7) )) )
					continue;
			}
			else
			{
				// FIXME: if an ent has a model and a sound, but isn't
				// in the PVS, only the PHS, clear the model

				//r1: will this even happen? ent->s.sound attenuates rapidly, it's very unlikely
				//something with a sound and not in PVS would still be hearable...

				/*if (ent->s.sound)
				{
					bitvector = fatpvs;	//clientphs;
				}
				else*/
					bitvector = fatpvs;

				if (ent->num_clusters == -1)
				{	// too many leafs for individual check, go by headnode
					if (!CM_HeadnodeVisible (ent->headnode, bitvector))
						continue;
					c_fullsend++;
				}
				else
				{	// check individual leafs
					for (i=0 ; i < ent->num_clusters ; i++)
					{
						l = ent->clusternums[i];
						if (bitvector[l >> 3] & (1 << (l&7) ))
							break;
					}
					if (i == ent->num_clusters)
						continue;		// not visible
				}

				if (ent->s.sound && !ent->s.modelindex && !ent->s.effects && !ent->s.event)
				{	
					// don't send sounds if they will be attenuated away
					vec3_t	delta;
					float	len;

					VectorSubtract (org, ent->s.origin, delta);
					len = VectorLength (delta);
					if (len > 400)
						continue;
				}
			}
		}

#if 0
		if (SV_AddProjectileUpdate(ent))
			continue; // added as a special projectile
#endif

		if (sv_nc_visibilitycheck->intvalue && !(sv_nc_clientsonly->intvalue && !ent->client) && ent->solid != SOLID_BSP && ent->solid != SOLID_TRIGGER)
		{
			// *********** NiceAss Start ************
			VectorCopy(org, start);
#ifdef ENHANCED_SERVER
			visible = SV_CheckPlayerVisible(((struct gclient_new_s *)(clent->client))->ps.viewangles, start, ent, true, false);
#else
			visible = SV_CheckPlayerVisible(((struct gclient_old_s *)(clent->client))->ps.viewangles, start, ent, true, false);
#endif
		
			if (!visible) {
				VectorCopy(org, start);

				// If the first direct check didn't see the player, check a little ahead
				// of yourself based on your current velocity, lag, frame update speed (100ms). 
				// This will compensate for clients predicting where they will be due to lag
				// (cl_predict)
				for (i = 0; i < 3; i++)
#ifdef ENHANCED_SERVER
					start[i] += ((struct gclient_new_s *)(clent->client))->ps.pmove.velocity[i] * 0.125f * ( 0.15f + (float)((struct gclient_new_s *)(clent->client))->ping * 0.001f );
#else
					start[i] += ((struct gclient_old_s *)(clent->client))->ps.pmove.velocity[i] * 0.125f * ( 0.15f + (float)((struct gclient_old_s *)(clent->client))->ping * 0.001f );
#endif
					

#ifdef ENHANCED_SERVER
			visible = SV_CheckPlayerVisible(((struct gclient_new_s *)(clent->client))->ps.viewangles, start, ent, false, true);
#else
			visible = SV_CheckPlayerVisible(((struct gclient_old_s *)(clent->client))->ps.viewangles, start, ent, false, true);
#endif

				if ( !visible ) {
					VectorCopy(org, start);
					// If the first/second direct check didn't see the player, check a little above
					// of yourself based on your current velocity. This will compensate for
					// clients predicting where they will be due to lag (cl_predict)
					start[2] += ent->maxs[2];
#ifdef ENHANCED_SERVER
					visible = SV_CheckPlayerVisible(((struct gclient_new_s *)(clent->client))->ps.viewangles, start, ent, false, true);
#else
					visible = SV_CheckPlayerVisible(((struct gclient_old_s *)(clent->client))->ps.viewangles, start, ent, false, true);
#endif
				}
			}

			// Don't send player at all. 100% secure but no footsteps unless you see the person.
			if (!visible && sv_nc_visibilitycheck->intvalue == 2)
				continue;

			// Don't send player at all IF there are no events/sounds/etc (like footsteps!) even
			// if the visibilitycheck is "1" and not "2". Hopefully harder on wallhackers.
			if (!visible && sv_nc_visibilitycheck->intvalue == 1 &&
				!ent->s.effects && !ent->s.sound && !ent->s.event)
				continue;
		} else {
			visible = true;
		}
		// ***********  NiceAss End  ************


		// add it to the circular client_entities array
		state = &svs.client_entities[svs.next_client_entities%svs.num_client_entities];
		if (ent->s.number != e)
		{
			//Com_DPrintf ("FIXING ENT->S.NUMBER!!!\n");
			//Com_Error (ERR_DROP, "Bad entity state on entity %d", e);
			if (sv_gamedebug->intvalue)
				Com_Printf ("GAME WARNING: Entity state on entity %d corrupted (bad ent->s.number %d)\n", LOG_SERVER|LOG_GAMEDEBUG|LOG_WARNING, e, ent->s.number);

			ent->s.number = e;
		}
		*state = ent->s;

		// *********** NiceAss Start ************
		// Send the entity, but don't associate a model with it. Less secure than sv_nc_visibilitycheck 2
		// but you can hear footsteps. Default functionality.
		if (!visible && sv_nc_visibilitycheck->intvalue == 1) {
			// Remove any model associations. Invisible.
			state->modelindex = state->modelindex2 = state->modelindex3 = state->modelindex4 = 0;
	
			// I think this holds the weapon for VWEP
			state->skinnum = 0;
		}
		// ***********  NiceAss End  ************

		// don't mark players missiles as solid
		if (ent->owner == client->edict)
			state->solid = 0;

		//r1: extended uptime fix (hmm, changing to unsigned should work just as well)
		svs.next_client_entities++;
		//if (++svs.next_client_entities == svs.num_client_entities)
		//	svs.next_client_entities = 0;

		//Com_Printf ("next will be %d, should be %d\n", 1 % svs.num_client_entities, svs.next_client_entities%svs.num_client_entities);

		//r1: break out at 128 ents since the client renderer dll can only process 128 anyway...
		if (++frame->num_entities > 128)
			break;
	}
}


/*
==================
SV_RecordDemoMessage

Save everything in the world out without deltas.
Used for recording footage for merged or assembled demos
==================
*/
void SV_RecordDemoMessage (void)
{
	int			e;
	edict_t		*ent;
	sizebuf_t	buf;
	byte		buf_data[32768];
	int			len;

	if (!svs.demofile)
		return;

	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// write a frame message that doesn't contain a player_state_t
	SZ_WriteByte (&buf, svc_frame);
	SZ_WriteLong (&buf, sv.framenum);

	SZ_WriteByte (&buf, svc_packetentities);

	e = 1;
	ent = EDICT_NUM(e);
	while (e < ge->num_edicts) 
	{
		// ignore ents without visible models unless they have an effect
		if (ent->inuse &&
			ent->s.number && 
			(ent->s.modelindex || ent->s.effects || ent->s.sound || ent->s.event) && 
			!(ent->svflags & SVF_NOCLIENT))
		{
			MSG_WriteDeltaEntity (&null_entity_state, &ent->s, false, true, ORIGINAL_PROTOCOL_VERSION);
			MSG_EndWriting (&buf);
		}

		e++;
		ent = EDICT_NUM(e);
	}

	SZ_WriteShort (&buf, 0);		// end of packetentities

	// now add the accumulated multicast information
	SZ_Write (&buf, svs.demo_multicast.data, svs.demo_multicast.cursize);
	SZ_Clear (&svs.demo_multicast);

	// now write the entire message to the file, prefixed by the length
	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, svs.demofile);
	fwrite (buf.data, buf.cursize, 1, svs.demofile);
}
