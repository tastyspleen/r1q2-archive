#include "client.h"
#include "localent.h"

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define	STOP_EPSILON	0.1

int ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float	backoff;
	float	change;
	int		i, blocked;
	
	blocked = 0;
	
	if (FLOAT_GT_ZERO(normal[2]))
		blocked |= 1;		// floor

	if (!normal[2])
		blocked |= 2;		// step
	
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}
	
	return blocked;
}

void CL_ClipMoveToEntities ( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr );
void LE_Physics_Toss (localent_t *ent)
{
	trace_t tr;
	vec3_t	vel;

	//copy old origin
	VectorCopy (ent->ent.origin, ent->ent.oldorigin);

	//FIXME: properly scale this to client FPS.
	ent->velocity[2] -= 1;

	//scale velocity based on something stupid
	VectorCopy (ent->velocity, vel);
	VectorScale (vel, cl.lerpfrac, vel);

	//add velocity to origin
	VectorAdd (ent->ent.origin, vel, ent->ent.origin);

	//check we didn't hit the world
	tr = CM_BoxTrace (ent->ent.oldorigin, ent->ent.origin, ent->mins, ent->maxs, 0, MASK_SOLID);
	if (tr.fraction != 1.0) {
		//vec3_t down;
		//if we did, back off.
		VectorCopy (tr.endpos, ent->ent.origin);
		if (ent->touch)
			ent->touch (ent, &tr.plane, tr.surface);

		//check for stop
		ClipVelocity (ent->velocity, tr.plane.normal, ent->velocity, ent->movetype == MOVETYPE_BOUNCE ? 1.7f : 1.0f);

		if ((tr.plane.normal[2] > 0.7 && ent->movetype == MOVETYPE_TOSS) || ((ent->velocity[2] < 0.01 && ent->velocity[2] > -0.01) && ent->movetype == MOVETYPE_BOUNCE))
			ent->movetype = MOVETYPE_NONE;
	}

	//note!! this only clips to entities that we currently know about (as the client).
	//this means if a localent is outside the PVS of a client, it will only clip to
	//the world.

	//fixme: do we want this?
	/*CL_ClipMoveToEntities (ent->ent.oldorigin, ent->mins, ent->maxs, ent->ent.origin, &tr);
	if (tr.ent) {
		if (ent->touch)
			ent->touch (ent, &tr.plane, tr.surface);
		
		VectorCopy (tr.endpos, ent->ent.origin);

		//check for stop
		ClipVelocity (ent->velocity, tr.plane.normal, ent->velocity, ent->movetype == MOVETYPE_BOUNCE ? 1.7 : 1);

		if (tr.plane.normal[2] > 0.7 && ent->movetype == MOVETYPE_TOSS || ((ent->velocity[2] < 0.01 && ent->velocity[2] > -0.01) && ent->movetype == MOVETYPE_BOUNCE))
			ent->movetype = MOVETYPE_NONE;

	}*/
}

/*void LE_Physics_Fly (localent_t *ent)
{

}*/

void LE_RunEntity (localent_t *ent)
{
	switch (ent->movetype)
	{
		case MOVETYPE_NONE:
			break;
		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
			LE_Physics_Toss (ent);
			break;
		case MOVETYPE_FLYMISSILE:
			//LE_Physics_Fly (ent);
			break;
		default:
			Com_Printf ("LE_RunEntity: bad movetype %i", LOG_CLIENT, ent->movetype);
	}

	if (ent->think && cl.time >= ent->nextthink )
		ent->think (ent);

	//if (ent->ent.flags & EF_GIB)
	//	CL_DiminishingTrail (ent->ent.oldorigin, ent->ent.origin, ent->ent, ent->ent.flags);
}
