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
=============
SV_EmitPacketEntities

Writes a delta update of an entity_state_t list to the message.
=============
*/
static void SV_EmitPacketEntities (client_t *cl, client_frame_t /*@null@*/*from, client_frame_t *to, sizebuf_t *msg)
{
	entity_state_t	*oldent, *newent;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		from_num_entities;
	int		bits;

#if 0
	if (numprojs)
		MSG_BeginWriteByte (msg, svc_packetentities2);
	else
#endif

	MSG_BeginWriteByte (msg, svc_packetentities);

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
		//XXX: this breaks delta parsing!!
		if (msg->cursize > ((MAX_MSGLEN - 100) - cl->datagram.cursize))
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

			MSG_WriteDeltaEntity (NULL, oldent, newent, msg, false, newent->number <= maxclients->value, 0, cl->protocol);

			oldindex++;
			newindex++;
			continue;
		}
	
		if (newnum < oldnum)
		{	// this is a new entity, send it from the baseline
			MSG_WriteDeltaEntity (NULL, &cl->lastlines[newnum], newent, msg, true, true, 0, cl->protocol);

			newindex++;
			continue;
		}

		if (newnum > oldnum)
		{	// the old entity isn't present in the new message
			//Com_Printf ("server: remove!!!\n");
			bits = U_REMOVE;
			if (oldnum >= 256)
				bits |= U_NUMBER16 | U_MOREBITS1;

			MSG_WriteByte (msg,	bits&255 );
			if (bits & 0x0000ff00)
				MSG_WriteByte (msg,	(bits>>8)&255 );

			if (bits & U_NUMBER16)
				MSG_WriteShort (msg, oldnum);
			else
				MSG_WriteByte (msg, oldnum);

			oldindex++;
			continue;
		}
	}

	//if (msg->cursize > 600) {
	//}

	MSG_WriteShort (msg, 0);	// end of packetentities

#if 0
	if (numprojs)
		SV_EmitProjectileUpdate(msg);
#endif
}



/*
=============
SV_WritePlayerstateToClient

=============
*/
static void SV_WritePlayerstateToClient (client_frame_t /*@null@*/*from, client_frame_t *to, sizebuf_t *msg)//, client_t *client)
{
	int						i;
	int						pflags;
	//player_state_t		*new_ps, *old_ps, *ops;
	//union player_state_t	*u_new, *u_old;
	player_state_new		dummy;
	int						statbits;

	player_state_new		*ps, *ops;

	ps = &to->ps;

	if (!from) {
		memset (&dummy, 0, sizeof(dummy));
		ops = &dummy;
	} else {
		ops = &from->ps;
	}

	//
	// determine what needs to be sent
	//
	pflags = 0;

	if (ps->pmove.pm_type != ops->pmove.pm_type)
		pflags |= PS_M_TYPE;

	if (ps->pmove.origin[0] != ops->pmove.origin[0]
		|| ps->pmove.origin[1] != ops->pmove.origin[1]
		|| ps->pmove.origin[2] != ops->pmove.origin[2] )
		pflags |= PS_M_ORIGIN;

	if (ps->pmove.velocity[0] != ops->pmove.velocity[0]
		|| ps->pmove.velocity[1] != ops->pmove.velocity[1]
		|| ps->pmove.velocity[2] != ops->pmove.velocity[2] )
		pflags |= PS_M_VELOCITY;

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


	if (ps->viewoffset[0] != ops->viewoffset[0]
		|| ps->viewoffset[1] != ops->viewoffset[1]
		|| ps->viewoffset[2] != ops->viewoffset[2] )
		pflags |= PS_VIEWOFFSET;

	if (ps->viewangles[0] != ops->viewangles[0]
		|| ps->viewangles[1] != ops->viewangles[1]
		|| ps->viewangles[2] != ops->viewangles[2] )
		pflags |= PS_VIEWANGLES;

	if (ps->kick_angles[0] != ops->kick_angles[0]
		|| ps->kick_angles[1] != ops->kick_angles[1]
		|| ps->kick_angles[2] != ops->kick_angles[2] )
		pflags |= PS_KICKANGLES;

	if (ps->blend[0] != ops->blend[0]
		|| ps->blend[1] != ops->blend[1]
		|| ps->blend[2] != ops->blend[2]
		|| ps->blend[3] != ops->blend[3] )
		pflags |= PS_BLEND;

	if (ps->fov != ops->fov)
		pflags |= PS_FOV;

	if (ps->rdflags != ops->rdflags)
		pflags |= PS_RDFLAGS;

	if (ps->gunframe != ops->gunframe)
		pflags |= PS_WEAPONFRAME;

	//only possibly send bbox if the client supports it AND the game is supposed to send it
#ifdef ENHANCED_SERVER
	if (client->protocol == ENHANCED_PROTOCOL_VERSION && (!VectorCompare (ps->mins, ops->mins) || !VectorCompare (ps->maxs, ops->maxs)))
		pflags |= PS_BBOX;
#endif

	pflags |= PS_WEAPONINDEX;

	//
	// write it
	//
	MSG_BeginWriteByte (msg, svc_playerinfo);
	MSG_WriteShort (msg, pflags);

	//
	// write the pmove_state_t
	//
	if (pflags & PS_M_TYPE)
		MSG_WriteByte (msg, ps->pmove.pm_type);

	if (pflags & PS_M_ORIGIN)
	{
		MSG_WriteShort (msg, ps->pmove.origin[0]);
		MSG_WriteShort (msg, ps->pmove.origin[1]);
		MSG_WriteShort (msg, ps->pmove.origin[2]);
	}

	if (pflags & PS_M_VELOCITY)
	{
		MSG_WriteShort (msg, ps->pmove.velocity[0]);
		MSG_WriteShort (msg, ps->pmove.velocity[1]);
		MSG_WriteShort (msg, ps->pmove.velocity[2]);
	}

	if (pflags & PS_M_TIME)
		MSG_WriteByte (msg, ps->pmove.pm_time);

	if (pflags & PS_M_FLAGS)
		MSG_WriteByte (msg, ps->pmove.pm_flags);

	if (pflags & PS_M_GRAVITY)
		MSG_WriteShort (msg, ps->pmove.gravity);

	if (pflags & PS_M_DELTA_ANGLES)
	{
		MSG_WriteShort (msg, ps->pmove.delta_angles[0]);
		MSG_WriteShort (msg, ps->pmove.delta_angles[1]);
		MSG_WriteShort (msg, ps->pmove.delta_angles[2]);
	}

	//
	// write the rest of the player_state_t
	//
	if (pflags & PS_VIEWOFFSET)
	{
		MSG_WriteChar (msg, ps->viewoffset[0]*4);
		MSG_WriteChar (msg, ps->viewoffset[1]*4);
		MSG_WriteChar (msg, ps->viewoffset[2]*4);
	}

	if (pflags & PS_VIEWANGLES)
	{
		MSG_WriteAngle16 (msg, ps->viewangles[0]);
		MSG_WriteAngle16 (msg, ps->viewangles[1]);
		MSG_WriteAngle16 (msg, ps->viewangles[2]);
	}

	if (pflags & PS_KICKANGLES)
	{
		MSG_WriteByte (msg, (byte)(ps->kick_angles[0]*4) & 0xFF);
		MSG_WriteByte (msg, (byte)(ps->kick_angles[1]*4) & 0xFF);
		MSG_WriteByte (msg, (byte)(ps->kick_angles[2]*4) & 0xFF);
	}

	if (pflags & PS_WEAPONINDEX)
	{
		MSG_WriteByte (msg, ps->gunindex);
	}

	if (pflags & PS_WEAPONFRAME)
	{
		MSG_WriteByte (msg, ps->gunframe);
		MSG_WriteChar (msg, ps->gunoffset[0]*4);
		MSG_WriteChar (msg, ps->gunoffset[1]*4);
		MSG_WriteChar (msg, ps->gunoffset[2]*4);
		MSG_WriteChar (msg, ps->gunangles[0]*4);
		MSG_WriteChar (msg, ps->gunangles[1]*4);
		MSG_WriteChar (msg, ps->gunangles[2]*4);
	}

	if (pflags & PS_BLEND)
	{
		MSG_WriteByte (msg, ps->blend[0]*255);

		//r1: fix byte overflow making this lower than it was supposed to be
		if (ps->blend[1] > 1)
			ps->blend[1] = 1;
		if (ps->blend[2] > 1)
			ps->blend[2] = 1;
		if (ps->blend[3] > 1)
			ps->blend[3] = 1;
		MSG_WriteByte (msg, ps->blend[1]*255);
		MSG_WriteByte (msg, ps->blend[2]*255);
		MSG_WriteByte (msg, ps->blend[3]*255);
	}

	if (pflags & PS_FOV)
		MSG_WriteByte (msg, ps->fov);

	if (pflags & PS_RDFLAGS)
		MSG_WriteByte (msg, ps->rdflags);

	if (pflags & PS_BBOX) {
		int j, k;
		int solid;

		i = ps->maxs[0]/8;
		if (i<1)
			i = 1;
		if (i>31)
			i = 31;

		// z is not symetric
		j = (-ps->mins[2])/8;
		if (j<1)
			j = 1;
		if (j>31)
			j = 31;

		// and z maxs can be negative...
		k = (ps->maxs[2]+32)/8;
		if (k<1)
			k = 1;
		if (k>63)
			k = 63;

		solid = (k<<10) | (j<<5) | i;

		MSG_WriteShort (msg, solid);
	}

	// send stats
	statbits = 0;
	for (i=0 ; i<MAX_STATS ; i++)
		if (ps->stats[i] != ops->stats[i])
			statbits |= 1<<i;
	MSG_WriteLong (msg, statbits);
	for (i=0 ; i<MAX_STATS ; i++)
		if (statbits & (1<<i) )
			MSG_WriteShort (msg, ps->stats[i]);
}


/*
==================
SV_WriteFrameToClient
==================
*/
void SV_WriteFrameToClient (client_t *client, sizebuf_t *msg)
{
	client_frame_t		*frame, *oldframe;
	int					lastframe;

//Com_Printf ("%i -> %i\n", client->lastframe, sv.framenum);
	// this is the frame we are creating
	frame = &client->frames[sv.framenum & UPDATE_MASK];

	if (client->lastframe <= 0)
	{	// client is asking for a retransmit
		oldframe = NULL;
		lastframe = -1;
	}
	else if (sv.framenum - client->lastframe >= (UPDATE_BACKUP - 3) )
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

	MSG_BeginWriteByte (msg, svc_frame);
	MSG_WriteLong (msg, sv.framenum);
	MSG_WriteLong (msg, lastframe);	// what we are delta'ing from
	MSG_WriteByte (msg, client->surpressCount);	// rate dropped packets
	client->surpressCount = 0;

	// send over the areabits
	MSG_WriteByte (msg, frame->areabytes);
	SZ_Write (msg, frame->areabits, frame->areabytes);

	// delta encode the playerstate
	SV_WritePlayerstateToClient (oldframe, frame, msg);//, client);

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

	for (i=0 ; i<3 ; i++)
	{
		mins[i] = org[i] - 8;
		maxs[i] = org[i] + 8;
	}

	count = CM_BoxLeafnums (mins, maxs, leafs, 64, NULL);
	if (count < 1)
		Com_Error (ERR_FATAL, "SV_FatPVS: count < 1");
	longs = (CM_NumClusters()+31)>>5;

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
			((long *)fatpvs)[j] |= ((long *)src)[j];
	}
}

static qboolean SV_CheckPlayerVisible(vec3_t Angles, vec3_t start, edict_t *ent, qboolean fullCheck, qboolean predictEnt) {
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
	int		e, i;
	vec3_t	org;
	edict_t	*ent;
	edict_t	*clent;
	client_frame_t	*frame;
	entity_state_t	*state;
	int		l;
	int		clientarea, clientcluster;
	int		leafnum;
	int		c_fullsend;
	byte	*clientphs;
	byte	*bitvector;

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
	frame = &client->frames[sv.framenum & UPDATE_MASK];

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
			org[i] = ((struct gclient_old_s *)(clent->client))->ps.pmove.origin[i]*0.125 + ((struct gclient_old_s *)(clent->client))->ps.viewoffset[i];
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

				if (!ent->s.modelindex)
				{	// don't send sounds if they will be attenuated away
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

		if (sv_nc_visibilitycheck->value && !(sv_nc_clientsonly->value && !ent->client)) {
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
			if (!visible && sv_nc_visibilitycheck->value == 2)
				continue;

			// Don't send player at all IF there are no events/sounds/etc (like footsteps!) even
			// if the visibilitycheck is "1" and not "2". Hopefully harder on wallhackers.
			if (!visible && sv_nc_visibilitycheck->value == 1 &&
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
			Com_DPrintf ("FIXING ENT->S.NUMBER!!!\n");
			ent->s.number = e;
		}
		*state = ent->s;

		// *********** NiceAss Start ************
		// Send the entity, but don't associate a model with it. Less secure than sv_nc_visibilitycheck 2
		// but you can hear footsteps. Default functionality.
		if (!visible && sv_nc_visibilitycheck->value == 1) {
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
	MSG_BeginWriteByte (&buf, svc_frame);
	MSG_WriteLong (&buf, sv.framenum);

	MSG_BeginWriteByte (&buf, svc_packetentities);

	e = 1;
	ent = EDICT_NUM(e);
	while (e < ge->num_edicts) 
	{
		// ignore ents without visible models unless they have an effect
		if (ent->inuse &&
			ent->s.number && 
			(ent->s.modelindex || ent->s.effects || ent->s.sound || ent->s.event) && 
			!(ent->svflags & SVF_NOCLIENT))
			MSG_WriteDeltaEntity (NULL, &null_entity_state, &ent->s, &buf, false, true, false, ENHANCED_PROTOCOL_VERSION);

		e++;
		ent = EDICT_NUM(e);
	}

	MSG_WriteShort (&buf, 0);		// end of packetentities

	// now add the accumulated multicast information
	SZ_Write (&buf, svs.demo_multicast.data, svs.demo_multicast.cursize);
	SZ_Clear (&svs.demo_multicast);

	// now write the entire message to the file, prefixed by the length
	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, svs.demofile);
	fwrite (buf.data, buf.cursize, 1, svs.demofile);
}
