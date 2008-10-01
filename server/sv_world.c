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
// world.c -- world query functions
#include "server.h"

/*
===============================================================================

ENTITY AREA CHECKING

FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define	STRUCT_FROM_LINK(l,t,m) ((t *)((byte *)l - (ptrdiff_t)&(((t *)0)->m)))

#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct	areanode_s *children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

unsigned int		sv_tracecount;

static areanode_t	sv_areanodes[AREA_NODES];
static int			sv_numareanodes;

static const float		*area_mins, *area_maxs;
static edict_t	**area_list;
static int		area_count, area_maxcount;
static int		area_type;

static int SV_HullForEntity (const edict_t *ent);


// ClearLink is used for new headnodes
__inline void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

__inline void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

__inline void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============
SV_CreateAreaNode

Builds a uniformly subdivided tree for the given world size
===============
*/
static areanode_t *SV_CreateAreaNode (int depth, const vec3_t mins, const vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);
	
	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;
	
	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);

	FastVectorCopy (*mins, mins1);	
	FastVectorCopy (*mins, mins2);	
	FastVectorCopy (*maxs, maxs1);	
	FastVectorCopy (*maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.models[1]->mins, sv.models[1]->maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void EXPORT SV_UnlinkEdict (edict_t *ent)
{
	if (!ent)
	{
		Com_Printf ("GAME ERROR: SV_UnlinkEdict: NULL pointer\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG);
		if (sv_gamedebug->intvalue > 1)
			Sys_DebugBreak ();
		return;
	}

	if (!ent->area.prev)
	{
		if (ent->solid != SOLID_NOT)
		{
			if (sv_gamedebug->intvalue)
			{
				Com_Printf ("GAME WARNING: SV_UnlinkEdict: unlinking entity %d that isn't linked\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, NUM_FOR_EDICT(ent));

				if (sv_gamedebug->intvalue >= 4)
					Sys_DebugBreak ();
			}
		}
		return;		// not linked in anywhere
	}
	RemoveLink (&ent->area);
	ent->area.prev = ent->area.next = NULL;
}


/*
===============
SV_LinkEdict

===============
*/
#define MAX_TOTAL_ENT_LEAFS		128

void EXPORT SV_LinkEdict (edict_t *ent)
{
	areanode_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			topnode;
	int			edict_number;

	if (!ent)
	{
		Com_Printf ("GAME ERROR: SV_LinkEdict: NULL pointer\n", LOG_SERVER|LOG_ERROR|LOG_GAMEDEBUG);
		if (sv_gamedebug->intvalue > 1)
			Sys_DebugBreak ();
		return;
	}

	if (ent->area.prev)
		SV_UnlinkEdict (ent);	// unlink from old position
		
	if (ent == ge->edicts)
		return;		// don't add the world

	if (!ent->inuse)
	{
		if (sv_gamedebug->intvalue)
		{
			Com_Printf ("GAME WARNING: SV_LinkEdict: linking entity %d that isn't in use\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, NUM_FOR_EDICT(ent));

			if (sv_gamedebug->intvalue >= 4)
				Sys_DebugBreak ();
		}
		return;
	}

	// set the size
	VectorSubtract (ent->maxs, ent->mins, ent->size);

	edict_number = NUM_FOR_EDICT(ent);

	//check the game dll didn't mix up s.solid / solid
	if (sv_gamedebug->intvalue)
	{
		if (ent->s.solid == SOLID_BBOX || ent->s.solid == SOLID_BSP || ent->s.solid == SOLID_TRIGGER)
		{
			Com_Printf ("GAME WARNING: SV_LinkEdict: entity %d has unusual s.solid value, did you mean to assign to solid?\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, edict_number);

			if (sv_gamedebug->intvalue >= 4)
				Sys_DebugBreak ();
		}
	}

	
	// encode the size into the entity_state for client prediction
	if (ent->solid == SOLID_BBOX && !((ent->svflags & SVF_DEADMONSTER) || (sv_new_entflags->intvalue && (ent->svflags & SVF_NOPREDICTION))))
	{	
		//special case projectile hack
		if (VectorCompare (ent->mins, vec3_origin) && VectorCompare (ent->maxs, vec3_origin))
		{
			ent->s.solid = 0;
			svs.entities[edict_number].solid2 = 0;
		}
		else
		{
			// assume that x/y are equal and symetric
			i = (int)(ent->maxs[0]/8);
			if (i<1)
				i = 1;
			if (i>31)
				i = 31;

			if (sv_gamedebug->intvalue)			
			{
				if ((float)i != (ent->maxs[0]/8))
				{
					Com_Printf ("GAME WARNING: Entity %d x/y bounding box mins/maxs (%.2f) does not lie on a quantization boundary (%.2f != %.2f)\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, edict_number, ent->maxs[0], (ent->maxs[0]/8), (float)i);
					if (sv_gamedebug->intvalue >= 5)
						Sys_DebugBreak ();
				}

				if (fabs (ent->maxs[0]) != fabs(ent->maxs[1]) || fabs(ent->mins[0]) != fabs(ent->mins[1]) || fabs (ent->maxs[0]) != fabs(ent->mins[0]))
				{
					Com_Printf ("GAME WARNING: Entity %d x/y bounding box mins/maxs are not equal and symmetric\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, edict_number);
					if (sv_gamedebug->intvalue >= 5)
						Sys_DebugBreak ();
				}
			}

			// z is not symetric
			j = (int)((-ent->mins[2])/8);
			if (j<1)
				j = 1;
			if (j>31)
				j = 31;

			if (sv_gamedebug->intvalue && (float)j != (-ent->mins[2])/8)
			{
				Com_Printf ("GAME WARNING: Entity %d z bounding box mins (%.2f) does not lie on a quantization boundary (%.2f != %.2f)\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, edict_number, ent->mins[2], (-ent->mins[2])/8, (float)j);
				if (sv_gamedebug->intvalue >= 5)
					Sys_DebugBreak ();
			}

			// and z maxs can be negative...
			k = (int)((ent->maxs[2]+32)/8);
			if (k<1)
				k = 1;
			if (k>63)
				k = 63;

			if (sv_gamedebug->intvalue && (float)k != ((ent->maxs[2]+32)/8))
			{
				//special hack to avoid spamming about crouched players
				if (!(edict_number - 1 < maxclients->intvalue && ent->maxs[2] == 4.0f))
				{
					Com_Printf ("GAME WARNING: Entity %d z bounding box maxs (%.2f) does not lie on a quantization boundary (%.2f != %.2f)\n", LOG_SERVER|LOG_WARNING|LOG_GAMEDEBUG, edict_number, ent->maxs[2], ((ent->maxs[2]+32)/8), (float)k);
					if (sv_gamedebug->intvalue >= 5)
						Sys_DebugBreak ();
				}
			}

			ent->s.solid = (k<<10) | (j<<5) | i;

			//now calculate our special value for R1Q2 protocol for full precision bboxes. NASTY HACK! since we can't
			//alter edict_t, there is a new private entity struct for the server :(.

			// assume that x/y are equal and symetric
			i = (int)(ent->maxs[0]);
			if (i<1)
				i = 1;
			if (i>255)
				i = 255;

			// z is not symetric
			j = (int)((-ent->mins[2]));
			if (j<1)
				j = 1;
			if (j>255)
				j = 255;

			// and z maxs can be negative...
			k = (int)((ent->maxs[2]+32768));
			if (k<1)
				k = 1;
			if (k>65535)	//2 bytes enough precision? :D
				k = 65535;

			svs.entities[edict_number].solid2 = (k<<16) | (j<<8) | i;
		}
	}
	else if (ent->solid == SOLID_BSP)
	{
		ent->s.solid = 31;		// a solid_bbox will never create this value
		svs.entities[edict_number].solid2 = 31;
	}
	else
	{
		ent->s.solid = 0;
		svs.entities[edict_number].solid2 = 0;
	}

	// set the abs box
	if (ent->solid == SOLID_BSP && 
	(ent->s.angles[0] || ent->s.angles[1] || ent->s.angles[2]) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v = (float)fabs( ent->mins[i]);
			if (v > max)
				max = v;
			v = (float)fabs( ent->maxs[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			ent->absmin[i] = ent->s.origin[i] - max;
			ent->absmax[i] = ent->s.origin[i] + max;
		}
	}
	else
	{	// normal
		VectorAdd (ent->s.origin, ent->mins, ent->absmin);	
		VectorAdd (ent->s.origin, ent->maxs, ent->absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->absmin[0] -= 1;
	ent->absmin[1] -= 1;
	ent->absmin[2] -= 1;
	ent->absmax[0] += 1;
	ent->absmax[1] += 1;
	ent->absmax[2] += 1;

// link to PVS leafs
	ent->num_clusters = 0;
	ent->areanum = 0;
	ent->areanum2 = 0;

	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (ent->absmin, ent->absmax,
		leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i=0 ; i<num_leafs ; i++)
	{
		clusters[i] = CM_LeafCluster (leafs[i]);
		area = CM_LeafArea (leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->areanum && ent->areanum != area)
			{
				if (ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading)
					Com_DPrintf ("Object touching 3 areas at %f %f %f\n",
					ent->absmin[0], ent->absmin[1], ent->absmin[2]);
				ent->areanum2 = area;
			}
			else
				ent->areanum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{	// assume we missed some leafs, and mark by headnode
		ent->num_clusters = -1;
		ent->headnode = topnode;
	}
	else
	{
		ent->num_clusters = 0;
		for (i=0 ; i<num_leafs ; i++)
		{
			if (clusters[i] == -1)
				continue;		// not a visible leaf
			for (j=0 ; j<i ; j++)
				if (clusters[j] == clusters[i])
					break;
			if (j == i)
			{
				if (ent->num_clusters == MAX_ENT_CLUSTERS)
				{	// assume we missed some leafs, and mark by headnode
					ent->num_clusters = -1;
					ent->headnode = topnode;
					break;
				}

				ent->clusternums[ent->num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if (!ent->linkcount)
	{
		FastVectorCopy (ent->s.origin, ent->s.old_origin);
	}
	ent->linkcount++;

	if (ent->solid == SOLID_NOT)
		return;

// find the first node that the ent's box crosses
	node = sv_areanodes;
	for (;;)
	{
		if (node->axis == -1)
			break;
		if (ent->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}
	
	// link it in	
	if (ent->solid == SOLID_TRIGGER)
		InsertLinkBefore (&ent->area, &node->trigger_edicts);
	else
		InsertLinkBefore (&ent->area, &node->solid_edicts);

}

//int area_recursions = 0;

/*
====================
SV_AreaEdicts_r

====================
*/
static void SV_AreaEdicts_r (const areanode_t *node)
{
	link_t			*l, *next;
	const link_t	*start;
	edict_t			*check;

	// touch linked edicts
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else
		start = &node->trigger_edicts;

	for (l=start->next  ; l != start ; l = next)
	{
		next = l->next;
		check = EDICT_FROM_AREA(l);

		if (check->solid == SOLID_NOT)
			continue;		// deactivated
		if (check->absmin[0] > area_maxs[0]
		|| check->absmin[1] > area_maxs[1]
		|| check->absmin[2] > area_maxs[2]
		|| check->absmax[0] < area_mins[0]
		|| check->absmax[1] < area_mins[1]
		|| check->absmax[2] < area_mins[2])
			continue;		// not touching

		if (area_count == area_maxcount)
		{
			Com_Printf ("SV_AreaEdicts: MAXCOUNT\n", LOG_SERVER|LOG_WARNING);
			return;
		}

		area_list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1)
		return;		// terminal node

	// recurse down both sides
	if ( area_maxs[node->axis] > node->dist )
		SV_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		SV_AreaEdicts_r ( node->children[1] );
}

/*
================
SV_AreaEdicts
================
*/
int EXPORT SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **list,
	int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	//area_recursions = 0;
	SV_AreaEdicts_r (sv_areanodes);

	return area_count;
}


//===========================================================================

/*
=============
SV_PointContents
=============
*/
int EXPORT SV_PointContents (vec3_t p)
{
	edict_t		*touch[MAX_EDICTS], *hit;
	int			i, num;
	int			contents, c2;
	int			headnode;
//	float		*angles;

	// get base contents from world
	contents = CM_PointContents (p, sv.models[1]->headnode);

	// or in contents from all the other entities
	num = SV_AreaEdicts (p, p, touch, MAX_EDICTS, AREA_SOLID);

	for (i=0 ; i<num ; i++)
	{
		hit = touch[i];

		// might intersect, so do an exact clip
		headnode = SV_HullForEntity (hit);
		//angles = hit->s.angles;
		//if (hit->solid != SOLID_BSP)
		//	angles = vec3_origin;	// boxes don't rotate

		c2 = CM_TransformedPointContents (p, headnode, hit->s.origin, hit->s.angles);

		contents |= c2;
	}

	return contents;
}



typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	edict_t		*passedict;
	int			contentmask;
} moveclip_t;



/*
================
SV_HullForEntity

Returns a headnode that can be used for testing or clipping an
object of mins/maxs size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
static int SV_HullForEntity (const edict_t *ent)
{
	cmodel_t	*model;

// decide which clipping hull to use, based on the size
	if (ent->solid == SOLID_BSP)
	{	// explicit hulls in the BSP model
		model = sv.models[ ent->s.modelindex ];

		if (!model)
			Com_Error (ERR_FATAL, "MOVETYPE_PUSH with a non bsp model");

		return model->headnode;
	}

	// create a temp hull from bounding box sizes

	return CM_HeadnodeForBox (ent->mins, ent->maxs);
}


//===========================================================================

/*
====================
SV_ClipMoveToEntities

====================
*/
static void SV_ClipMoveToEntities (moveclip_t *clip )
{
	int			i, num;
	edict_t		*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;
	int			headnode;
	float		*angles;

	num = SV_AreaEdicts (clip->boxmins, clip->boxmaxs, touchlist
		, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i=0 ; i<num ; i++)
	{
		touch = touchlist[i];
		if (touch->solid == SOLID_NOT)
			continue;
		if (touch == clip->passedict)
			continue;
		if (clip->trace.allsolid)
			return;
		if (clip->passedict)
		{
		 	if (touch->owner == clip->passedict)
				continue;	// don't clip against own missiles
			if (clip->passedict->owner == touch)
				continue;	// don't clip against owner
		}

		if ( !(clip->contentmask & CONTENTS_DEADMONSTER)
		&& (touch->svflags & SVF_DEADMONSTER) )
				continue;

		// might intersect, so do an exact clip
		headnode = SV_HullForEntity (touch);
		angles = touch->s.angles;
		if (touch->solid != SOLID_BSP)
			angles = vec3_origin;	// boxes don't rotate

		if (touch->svflags & SVF_MONSTER)
			trace = CM_TransformedBoxTrace (clip->start, clip->end,
				clip->mins2, clip->maxs2, headnode, clip->contentmask,
				touch->s.origin, angles);
		else
			trace = CM_TransformedBoxTrace (clip->start, clip->end,
				clip->mins, clip->maxs, headnode,  clip->contentmask,
				touch->s.origin, angles);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
		 	if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if (trace.startsolid)
			clip->trace.startsolid = true;
	}
}


/*
==================
SV_TraceBounds
==================
*/
/*static void SV_TraceBounds (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	if (end[0] > start[0])
	{
		boxmins[0] = start[0] + mins[0] - 1;
		boxmaxs[0] = end[0] + maxs[0] + 1;
	}
	else
	{
		boxmins[0] = end[0] + mins[0] - 1;
		boxmaxs[0] = start[0] + maxs[0] + 1;
	}

	if (end[1] > start[1])
	{
		boxmins[1] = start[1] + mins[1] - 1;
		boxmaxs[1] = end[1] + maxs[1] + 1;
	}
	else
	{
		boxmins[1] = end[1] + mins[1] - 1;
		boxmaxs[1] = start[1] + maxs[1] + 1;
	}

	if (end[2] > start[2])
	{
		boxmins[2] = start[2] + mins[2] - 1;
		boxmaxs[2] = end[2] + maxs[2] + 1;
	}
	else
	{
		boxmins[2] = end[2] + mins[2] - 1;
		boxmaxs[2] = start[2] + maxs[2] + 1;
	}
}*/

/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.

Passedict and edicts owned by passedict are explicitly not checked.

==================
*/
trace_t EXPORT SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passedict, int contentmask)
{
	int			i;
	moveclip_t	clip;

	if (!mins)
		mins = vec3_origin;

	if (!maxs)
		maxs = vec3_origin;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	//r1: server-side hax for bad looping traces
	if (++sv_tracecount >= sv_max_traces_per_frame->intvalue)
	{
		Com_Printf ("GAME ERROR: Bad SV_Trace: %u calls in a single frame, aborting!\n", LOG_SERVER|LOG_GAMEDEBUG|LOG_ERROR, sv_tracecount);
		if (sv_gamedebug->intvalue >= 2)
			Sys_DebugBreak ();

		clip.trace.fraction = 1.0;
		clip.trace.ent = ge->edicts;
		FastVectorCopy (*end, clip.trace.endpos);
		//this is really nasty, attempts to overwrite source in Game DLL. may result in flying players and ents if it uses an origin
		//directly!! we may even crash here if we are given a write protected start.
		FastVectorCopy (*end, *start);
		sv_tracecount = 0;
		return clip.trace;
	}

	// clip to world
	clip.trace = CM_BoxTrace (start, end, mins, maxs, 0, contentmask);
	clip.trace.ent = ge->edicts;

	if (FLOAT_EQ_ZERO (clip.trace.fraction))
		return clip.trace;		// blocked by the world

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	FastVectorCopy (*mins, clip.mins2);
	FastVectorCopy (*maxs, clip.maxs2);
	
	// create the bounding box of the entire move
	//SV_TraceBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );
	for ( i=0 ; i<3 ; i++ )
	{
		if ( end[i] > start[i] )
		{
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		}
		else
		{
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}

	// clip to other solid entities
	SV_ClipMoveToEntities ( &clip );

	return clip.trace;
}
