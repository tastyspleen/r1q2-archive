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

#include "client.h"

/*
===================
CL_CheckPredictionError
===================
*/
void CL_CheckPredictionError (void)
{
	int		frame;
	int		delta[3];
	int		len;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cls.netchan.incoming_acknowledged;
	frame &= (CMD_BACKUP-1);

	// compare what the server returned with what we had predicted it to be
	VectorSubtract (cl.frame.playerstate.pmove.origin, cl.predicted_origins[frame], delta);

	// save the prediction error for interpolation
	len = abs(delta[0]) + abs(delta[1]) + abs(delta[2]);

	//r1: demos replay at 10 fps, falling could trigger this which looks like shit.
	if (len > (!cl.attractloop ? 640 : 1280))	// 80 world units
	{
		// a teleport or something
		VectorClear (cl.prediction_error);
	}
	else
	{
		if (cl_showmiss->intvalue && (delta[0] || delta[1] || delta[2]) )
			Com_Printf ("prediction miss on %i: %i (%.2f %.2f %.2f) != (%.2f %.2f %.2f)\n", LOG_CLIENT, cl.frame.serverframe, 
			delta[0] + delta[1] + delta[2],
			cl.frame.playerstate.pmove.origin[0] * 0.125f, cl.frame.playerstate.pmove.origin[1] * 0.125f, cl.frame.playerstate.pmove.origin[2] * 0.125f,
			cl.predicted_origins[frame][0] * 0.125f, cl.predicted_origins[frame][1] * 0.125f, cl.predicted_origins[frame][2] * 0.125f);

		VectorCopy (cl.frame.playerstate.pmove.origin, cl.predicted_origins[frame]);

		// save for error itnerpolation
		cl.prediction_error[0] = delta[0]*0.125f;
		cl.prediction_error[1] = delta[1]*0.125f;
		cl.prediction_error[2] = delta[2]*0.125f;
	}
}


/*
====================
CL_ClipMoveToEntities

====================
*/
void CL_ClipMoveToEntities (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr )
{
	int			i, x, zd, zu;
	trace_t		trace;
	int			headnode;
	float		*angles;
	entity_state_t	*ent;
	int			num;
	cmodel_t		*cmodel;
	vec3_t		bmins, bmaxs;

	//XXX: this breaks world clipping somehow...
	/*
	vec3_t		saved_mins, saved_maxs;

	VectorCopy (mins, saved_mins);
	VectorCopy (maxs, saved_maxs);

	if (1)
	{
		for (i=0 ; i<cl.frame.num_entities ; i++)
		{
			num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
			ent = &cl_parse_entities[num];
			
			if (ent->number == cl.playernum+1)
			{
				x = 8*(ent->solid & 31);
				zd = 8*((ent->solid>>5) & 31);
				zu = 8*((ent->solid>>10) & 63) - 32;

				bmins[0] = bmins[1] = -x;
				bmaxs[0] = bmaxs[1] = x;
				bmins[2] = -zd;
				bmaxs[2] = zu;

				VectorCopy (bmins, mins);
				VectorCopy (bmaxs, maxs);

				break;
			}
		}
	}*/

	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		if (!ent->solid)
			continue;

		if (ent->number == cl.playernum+1)
			continue;

		if (ent->solid == 31)
		{	// special value for bmodel
			cmodel = cl.model_clip[ent->modelindex];
			if (!cmodel)
				continue;
			headnode = cmodel->headnode;
			angles = ent->angles;
		}
		else
		{	
			// encoded bbox
			if (cls.protocolVersion >= MINOR_VERSION_R1Q2_32BIT_SOLID)
			{
				x = (ent->solid & 255);
				zd = ((ent->solid>>8) & 255);
				zu = ((ent->solid>>16) & 65535) - 32768;
			}
			else
			{
				x = 8*(ent->solid & 31);
				zd = 8*((ent->solid>>5) & 31);
				zu = 8*((ent->solid>>10) & 63) - 32;
			}

			bmins[0] = bmins[1] = -(float)x;
			bmaxs[0] = bmaxs[1] = (float)x;
			bmins[2] = -(float)zd;
			bmaxs[2] = (float)zu;

			headnode = CM_HeadnodeForBox (bmins, bmaxs);
			angles = vec3_origin;	// boxes don't rotate
		}

		if (tr->allsolid)
			return;

		trace = CM_TransformedBoxTrace (start, end,
			mins, maxs, headnode,  MASK_PLAYERSOLID,
			ent->origin, angles);

		if (trace.allsolid || trace.startsolid ||
		trace.fraction < tr->fraction)
		{
			trace.ent = (struct edict_s *)ent;
		 	if (tr->startsolid)
			{
				*tr = trace;
				tr->startsolid = true;
			}
			else
				*tr = trace;
		}
		else if (trace.startsolid)
			tr->startsolid = true;
	}

	//restore for pmove
	/*VectorCopy (saved_mins, mins);
	VectorCopy (saved_maxs, maxs);*/
}


/*
================
CL_PMTrace
================
*/
trace_t		EXPORT CL_PMTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t	t;

	// check against world
	t = CM_BoxTrace (start, end, mins, maxs, 0, MASK_PLAYERSOLID);//(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_DEADMONSTER));
	if (t.fraction < 1.0f)
		t.ent = (struct edict_s *)1;

	// check all other solid models
	CL_ClipMoveToEntities (start, mins, maxs, end, &t);

	return t;
}

int		EXPORT CL_PMpointcontents (vec3_t point)
{
	int			i;
	entity_state_t	*ent;
	int			num;
	cmodel_t		*cmodel;
	int			contents;

	contents = CM_PointContents (point, 0);

	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		if (ent->solid != 31) // special value for bmodel
			continue;

		cmodel = cl.model_clip[ent->modelindex];
		if (!cmodel)
			continue;

		contents |= CM_TransformedPointContents (point, cmodel->headnode, ent->origin, ent->angles);
	}

	return contents;
}

/*
=================
CL_PredictMovement

Sets cl.predicted_origin and cl.predicted_angles
=================
*/
void CL_PredictMovement (void)
{
	static int	last_step_frame = 0;
	int			ack, current;
	int			frame;
	int			oldframe;
	usercmd_t	*cmd;
	pmove_new_t		pm;
	int			step;
	int			oldz;

	if (cls.state != ca_active)
		return;

	if (cl_paused->intvalue)
		return;

	if (!cl_predict->intvalue || (cl.frame.playerstate.pmove.pm_flags & PMF_NO_PREDICTION))
	{
		cl.predicted_angles[0] = cl.viewangles[0] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[0]);
		cl.predicted_angles[1] = cl.viewangles[1] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[1]);
		cl.predicted_angles[2] = cl.viewangles[2] + SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[2]);
		return;
	}
	//Com_Printf ("predicting %d...\n",rand());

	ack = cls.netchan.incoming_acknowledged;
	current = cls.netchan.outgoing_sequence;

	// if we are too far out of date, just freeze
	if (current - ack >= CMD_BACKUP)
	{
		if (cl_showmiss->intvalue)
			Com_Printf ("exceeded CMD_BACKUP\n", LOG_CLIENT);
		return;	
	}

	// copy current state to pmove
	//memset (&pm, 0, sizeof(pm));
	
	/*pm.groundentity = NULL;
	pm.numtouch = 0;
	pm.snapinitial = false;
	pm.touchents = NULL;
	VectorClear (pm.viewangles);
	pm.viewheight = 0;
	pm.waterlevel = 0;
	pm.watertype = 0;*/

	pm.snapinitial = false;
	pm.trace = CL_PMTrace;
	pm.pointcontents = CL_PMpointcontents;
	pm.s = cl.frame.playerstate.pmove;

	VectorClear (pm.viewangles);

//	SCR_DebugGraph (current - ack - 1, 0);

	frame = 0;

	if (cl.enhancedServer)
	{
		//FastVectorCopy (cl.frame.playerstate.mins, pm.mins);
		//FastVectorCopy (cl.frame.playerstate.maxs, pm.maxs);
	}
	else
	{
		VectorSet (pm.mins, -16, -16, -24);
		VectorSet (pm.maxs,  16,  16, 32);
	}

	if (pm.s.pm_type == PM_SPECTATOR && cls.serverProtocol == PROTOCOL_R1Q2)
		pm.multiplier = 2;
	else
		pm.multiplier = 1;

	pm.enhanced = cl.enhancedServer;
	pm.strafehack = cl.strafeHack;

	if (cl_async->intvalue)
	{
		// run frames
		while (++ack <= current) //jec - changed '<' to '<=' cause current is our pending cmd.
		{
			frame = ack & (CMD_BACKUP-1);
			cmd = &cl.cmds[frame];

			if (!cmd->msec)
				continue; //jec - ignore 'null' usercmd entries.

			pm.cmd = *cmd;

			Pmove (&pm);

			// save for debug checking
			VectorCopy (pm.s.origin, cl.predicted_origins[frame]);
		}

		switch (cl_smoothsteps->intvalue)
		{
			case 3:
				//get immediate results of this prediction vs last one
				step = pm.s.origin[2] - (int)(cl.predicted_origin[2] * 8);

				//r1ch: treat only some units as steps
				if (((step > 62 && step < 66) || (step > 94 && step < 98) || (step > 126 && step < 130)) && !VectorCompare (pm.s.velocity, vec3_origin) && (pm.s.pm_flags & PMF_ON_GROUND))
				{
					cl.predicted_step = step * 0.125f;
					cl.predicted_step_time = cls.realtime - (int)(cls.frametime * 500);
				}
				break;

			case 2:
				//r1ch: make difference more aggressive
				ack--;
				/* intentional fall through */
			case 1:
				oldframe = (ack-2) & (CMD_BACKUP-1);
				oldz = cl.predicted_origins[oldframe][2];
				step = pm.s.origin[2] - oldz;

				if (last_step_frame != current && step > 63 && step < 160 && (pm.s.pm_flags & PMF_ON_GROUND) )
				{
					cl.predicted_step = step * 0.125f;
					cl.predicted_step_time = cls.realtime - (int)(cls.frametime * 500);
					last_step_frame = current;
				}
				break;
		}

	}
	else
	{
		// run frames
		while (++ack < current)
		{
			frame = ack & (CMD_BACKUP-1);
			cmd = &cl.cmds[frame];

			pm.cmd = *cmd;

			Pmove (&pm);

			// save for debug checking
			VectorCopy (pm.s.origin, cl.predicted_origins[frame]);
		}

		oldframe = (ack-2) & (CMD_BACKUP-1);
		oldz = cl.predicted_origins[oldframe][2];
		step = pm.s.origin[2] - oldz;
		if (step > 63 && step < 160 && (pm.s.pm_flags & PMF_ON_GROUND) )
		{
			cl.predicted_step = step * 0.125f;
			cl.predicted_step_time = cls.realtime - (int)(cls.frametime * 500);
		}
	}

	// copy results out for rendering
	cl.predicted_origin[0] = pm.s.origin[0]*0.125f;
	cl.predicted_origin[1] = pm.s.origin[1]*0.125f;
	cl.predicted_origin[2] = pm.s.origin[2]*0.125f;

	FastVectorCopy (pm.viewangles, cl.predicted_angles);
}
