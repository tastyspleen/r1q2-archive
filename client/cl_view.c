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
// cl_view.c -- player rendering positioning

#include "client.h"
#include "localent.h"

extern int deffered_model_index;

//=============
//
// development tools for weapons
//
int			gun_frame;
struct model_s	*gun_model;

//=============

cvar_t		*crosshair;
cvar_t		*cl_testparticles;
cvar_t		*cl_testentities;
cvar_t		*cl_testlights;
cvar_t		*cl_testblend;

cvar_t		*cl_stats;

cvar_t		*cl_drawfps;
cvar_t		*cl_stfu_ilkhan;
cvar_t		*cl_defermodels;

extern cvar_t		*scr_showturtle;

int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;


/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene (void)
{
	r_numdlights = 0;
	r_numentities = 0;
	r_numparticles = 0;
}

/*
=====================
V_AddEntity

=====================
*/
void V_AddEntity (entity_t *ent)
{
	if (r_numentities >= MAX_ENTITIES)
		return;
	r_entities[r_numentities++] = *ent;
}


/*
=====================
V_AddParticle

=====================
*/
void V_AddParticle (vec3_t org, int color, float alpha)
{
	particle_t	*p;

	if (r_numparticles >= MAX_PARTICLES)
		return;
	p = &r_particles[r_numparticles++];
	VectorCopy (org, p->origin);
	p->color = color;
	p->alpha = alpha;
}

/*
=====================
V_AddLight

=====================
*/
void V_AddLight (vec3_t org, float intensity, float r, float g, float b)
{
	dlight_t	*dl;

	if (r_numdlights >= MAX_DLIGHTS)
		return;
	dl = &r_dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->intensity = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


/*
=====================
V_AddLightStyle

=====================
*/
void V_AddLightStyle (int style, float r, float g, float b)
{
	lightstyle_t	*ls;

	if (style < 0 || style > MAX_LIGHTSTYLES)
		Com_Error (ERR_DROP, "Bad light style %i", style);
	ls = &r_lightstyles[style];

	ls->white = r+g+b;
	ls->rgb[0] = r;
	ls->rgb[1] = g;
	ls->rgb[2] = b;
}

/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
void V_TestParticles (void)
{
	particle_t	*p;
	int			i, j;
	float		d, r, u;

	r_numparticles = MAX_PARTICLES;
	for (i=0 ; i<r_numparticles ; i++)
	{
		d = i*0.25;
		r = 4*((i&7)-3.5);
		u = 4*(((i>>3)&7)-3.5);
		p = &r_particles[i];

		for (j=0 ; j<3 ; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*d +
			cl.v_right[j]*r + cl.v_up[j]*u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}

/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities (void)
{
	int			j;
	float		f, r;
	entity_t	*ent;
	localent_t	*lent;


	lent = Le_Alloc ();
	if (!lent)
		return;

	ent = &lent->ent;

	lent->movetype = MOVETYPE_TOSS;
	
	r = 64 * ( (0%4) - 1.5 );
	f = 64 * (0/4) + 128;

	for (j=0 ; j<3 ; j++)
		ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
		cl.v_right[j]*r;

	VectorSet (lent->mins, -16, -16, -16);
	VectorSet (lent->maxs, 16, 16, 32);

	lent->velocity[2] = 50;
	lent->velocity[1] = 20;
	lent->velocity[0] = 20;

	lent->movetype = MOVETYPE_BOUNCE;

	ent->model = cl.baseclientinfo.model;
	ent->skin = cl.baseclientinfo.skin;
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	dlight_t	*dl;
	vec3_t		fwd;
	trace_t		tr;
	vec3_t		temp;

	vec3_t right,up;

	r_numdlights = 1;
	memset (r_dlights, 0, sizeof(r_dlights));

	AngleVectors (cl.viewangles, fwd, right, up);

	VectorSet (temp, 0,0,0);
	//VectorCopy (cl.viewangles, forward);
	VectorScale (fwd, 2048, fwd);

	tr = CM_BoxTrace (cl.refdef.vieworg, fwd, temp, temp, 0, CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_DEADMONSTER);

	dl = &r_dlights[0];

	VectorCopy (tr.endpos, dl->origin);

	dl->color[0] = 1;
	dl->color[1] = 1;
	dl->color[2] = 1;
	dl->intensity = 200;
}

//===================================================================

/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh (void)
{
	char		mapname[MAX_QPATH];
	int			i;
//	char		name[MAX_QPATH];
	float		rotate;
	vec3_t		axis;

	if (!cl.configstrings[CS_MODELS+1][0])
		return;		// no map loaded

	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);

	// let the render dll load the map
	strcpy (mapname, cl.configstrings[CS_MODELS+1] + 5);	// skip "maps/"
	mapname[strlen(mapname)-4] = 0;		// cut off ".bsp"

	Cvar_ForceSet ("$mapname", mapname);

	// register models, pics, and skins
	Com_Printf ("Map: %s\r", mapname); 
	SCR_UpdateScreen ();
	re.BeginRegistration (mapname);
	Com_Printf ("                                     \r");

	Netchan_Transmit (&cls.netchan, 0, NULL);

	// precache status bar pics
	Com_Printf ("pics\r"); 
	SCR_UpdateScreen ();
	SCR_TouchPics ();
	Com_Printf ("                                     \r");

	CL_RegisterTEntModels ();

	num_cl_weaponmodels = 1;
	strcpy(cl_weaponmodels[0], "weapon.md2");

	for (i=1 ; i<MAX_MODELS && cl.configstrings[CS_MODELS+i][0] ; i++)
	{
		if (cl.configstrings[CS_MODELS+i][0] == '#')
		{
			// special player weapon model
			if (num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS)
			{
				strncpy(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS+i]+1,
					sizeof(cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
		}
	}

	Netchan_Transmit (&cls.netchan, 0, NULL);

	if (cl_defermodels->intvalue)
	{
		deffered_model_index = 1;
		for (i = 0; i < MAX_MODELS; i++)
		{
			cl.model_clip[i] = NULL;
			cl.model_draw[i] = NULL;
		}
	}
	else
	{
		deffered_model_index = MAX_MODELS;
		for (i = 0; i < MAX_MODELS; i++)
		{
			if (!cl.configstrings[CS_MODELS+i][0])
				continue;

			if (cl.configstrings[CS_MODELS+i][0] != '#')
			{
				cl.model_draw[i] = re.RegisterModel (cl.configstrings[CS_MODELS+i]);
				if (cl.configstrings[CS_MODELS+deffered_model_index][0] == '*')
					cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS+i]);
				else
					cl.model_clip[i] = NULL;
			}
		}
	}

	cl.model_draw[1] = re.RegisterModel (cl.configstrings[CS_MODELS+1]);
	if (cl.configstrings[CS_MODELS+1][0] == '*')
		cl.model_clip[1] = CM_InlineModel (cl.configstrings[CS_MODELS+1]);
	else
		cl.model_clip[1] = NULL;

	Com_Printf ("images\r"); 
	SCR_UpdateScreen ();
	for (i=1 ; i<MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0] ; i++)
	{
		cl.image_precache[i] = re.RegisterPic (cl.configstrings[CS_IMAGES+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	
	Com_Printf ("                                     \r");
	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_Printf ("client %i\r", i); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
		Com_Printf ("                                     \r");
	}

	Netchan_Transmit (&cls.netchan, 0, NULL);

	CL_LoadClientinfo (&cl.baseclientinfo, "");

	// set sky textures and speed
	Com_Printf ("sky\r", i); 
	SCR_UpdateScreen ();
	rotate = atof (cl.configstrings[CS_SKYROTATE]);
	sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f", 
		&axis[0], &axis[1], &axis[2]);
	re.SetSky (cl.configstrings[CS_SKY], rotate, axis);
	Com_Printf ("                                     \r");

	// the renderer can now free unneeded stuff
	re.EndRegistration ();

	// clear any lines of console text
	Con_ClearNotify ();

	SCR_UpdateScreen ();

	cl.refresh_prepped = true;
	cl.force_refdef = true;	// make sure we have a valid refdef

	//probably out of date
	cl.frame.valid = false;

	//reset current list
	cl.refdef.num_entities = 0;
	cl.refdef.entities = NULL;
	cl.refdef.num_particles = 0;
	cl.refdef.particles = NULL;
	cl.refdef.num_dlights = 0;
	cl.refdef.dlights = NULL;
	cl.refdef.lightstyles = 0;

	S_StopAllSounds ();

	//can't use cls.realtime - could be out of date :)
	cls.defer_rendering = Sys_Milliseconds() + (cl_defertimer->value * 1000);

	// start the cd track
#ifdef CD_AUDIO
	CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
#endif
}

/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	a;
	float	x;

	if (fov_x < 1 || fov_x > 179)
		Com_Error (ERR_DROP, "Bad fov: %f", fov_x);

	x = width/tan(fov_x/360*M_PI);

	a = atan (height/x);

	a = a*360/M_PI;

	return a;
}

//============================================================================

// gun frame debugging functions
void V_Gun_Next_f (void)
{
	gun_frame++;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Prev_f (void)
{
	gun_frame--;
	if (gun_frame < 0)
		gun_frame = 0;
	Com_Printf ("frame %i\n", gun_frame);
}

void V_Gun_Model_f (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		gun_model = NULL;
		return;
	}
	Com_sprintf (name, sizeof(name), "models/%s/tris.md2", Cmd_Argv(1));
	gun_model = re.RegisterModel (name);
}

//============================================================================


/*
=================
SCR_DrawCrosshair
=================
*/
void SCR_DrawCrosshair (void)
{
	if (!crosshair->intvalue)
		return;

	/*if (crosshair->modified)
	{
		crosshair->modified = false;
		SCR_TouchPics ();
	}*/

	if (!crosshair_pic[0])
		return;

	re.DrawPic (scr_vrect.x + ((scr_vrect.width - crosshair_width)>>1)
	, scr_vrect.y + ((scr_vrect.height - crosshair_height)>>1), crosshair_pic);
}

int spc = 0;
int fps = 0;
int frames_this_second = 0;
unsigned int frames_seconds = 0;

char rateMsg[] = "RATEDROP";
char frameMsg[] = "OLDFRAME";
char parseMsg[] = "OLDPARSE";
char overflowMsg[] = "OVERFLOW";

extern int __cdecl entitycmpfnc( const entity_t *, const entity_t * );

/*
==================
V_RenderView

==================
*/
#ifdef CL_STEREO_SUPPORT
void V_RenderView( float stereo_separation )
#else
void V_RenderView(void)
#endif
{
	if (cls.state != ca_active)
		return;

	if (!cl.refresh_prepped)
		return;			// still loading

	if (cl_timedemo->intvalue)
	{
		if (!cl.timedemo_start)
			cl.timedemo_start = Sys_Milliseconds ();
		cl.timedemo_frames++;
	}

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if ( cl.frame.valid && (cl.force_refdef || !cl_paused->intvalue) )
	{
		cl.force_refdef = false;

		V_ClearScene ();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_AddEntities ();

		if (cl_testparticles->intvalue)
			V_TestParticles ();

		if (cl_testentities->intvalue)
			V_TestEntities ();
		
		if (cl_testlights->intvalue)
			V_TestLights ();
		/*if (cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.5;
		}*/

		// offset vieworg appropriately if we're doing stereo separation
#ifdef CL_STEREO_SUPPORT
		if ( stereo_separation != 0 )
		{
			vec3_t tmp;

			VectorScale( cl.v_right, stereo_separation, tmp );
			VectorAdd( cl.refdef.vieworg, tmp, cl.refdef.vieworg );
		}
#endif

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/8 pixel, so add 1/16 in each axis
		cl.refdef.vieworg[0] += 1.0/16;
		cl.refdef.vieworg[1] += 1.0/16;
		cl.refdef.vieworg[2] += 1.0/16;

		cl.refdef.x = scr_vrect.x;
		cl.refdef.y = scr_vrect.y;
		cl.refdef.width = scr_vrect.width;
		cl.refdef.height = scr_vrect.height;
		cl.refdef.fov_y = CalcFov (cl.refdef.fov_x, cl.refdef.width, cl.refdef.height);
		cl.refdef.time = cl.time*0.001;

		cl.refdef.areabits = cl.frame.areabits;

		if (!cl_add_entities->intvalue)
			r_numentities = 0;
		if (!cl_add_particles->intvalue)
			r_numparticles = 0;
		if (!cl_add_lights->intvalue)
			r_numdlights = 0;
		if (!cl_add_blend->intvalue)
		{
			VectorClear (cl.refdef.blend);
		}

		if (cls.defer_rendering > cls.realtime)
			r_numentities = r_numparticles = r_numdlights = 0;

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;

		// sort entities for better cache locality
        qsort( cl.refdef.entities, cl.refdef.num_entities, sizeof( cl.refdef.entities[0] ), (int (__cdecl *)(const void *, const void *))entitycmpfnc );
	
		re.RenderFrame (&cl.refdef);
	}

	
	if (cl_stats->intvalue)
		Com_Printf ("ent:%i  lt:%i  part:%i\n", r_numentities, r_numdlights, r_numparticles);
	//if ( log_stats->value && ( log_stats_file != 0 ) )
	//	fprintf( log_stats_file, "%i,%i,%i,",r_numentities, r_numdlights, r_numparticles);

	if (Sys_Milliseconds () - frames_seconds >= 1000)
	{
		spc = serverPacketCount;
		fps = frames_this_second;
		frames_this_second = 0;
		serverPacketCount = 0;
		frames_seconds = Sys_Milliseconds();
	}

	if (cl_shownet->intvalue == -1)
	{
		char buff[16];
		int x;
		Com_sprintf (buff, sizeof(buff), "%d", spc);
		for (x = 0; x < strlen(buff); x++) {
			re.DrawChar (viddef.width-26+x*8, viddef.height / 2, 128 + buff[x]);
		}
	}

	if (cl_drawfps->intvalue)
	{
		int x;
		char buff[16];
		frames_this_second++;

		if (cl_drawfps->intvalue == 2) {
			fps = (int)(1.0 / cls.frametime);
		}
		Com_sprintf (buff, sizeof(buff), "%d", fps);
		for (x = 0; x < strlen(buff); x++) {
			re.DrawChar (viddef.width-26+x*8, viddef.height - 16, 128 + buff[x]);
		}
	}

	if (cl_stfu_ilkhan->intvalue) {
		char buff[16];
		int x;
		int secs = (cl.frame.serverframe % 600) / 10;
		int mins = cl.frame.serverframe / 600;
		
		Com_sprintf (buff, sizeof(buff), "%d:%.2d", mins, secs);
		for (x = 0; x < strlen(buff); x++) {
			re.DrawChar (x*8, viddef.height - 8, 128 + buff[x]);
		}
	}

	//FIXME: incorrect use of scr_ prefix
	if (scr_showturtle->intvalue)
	{
		frame_t *old;

		if (cl.surpressCount)
		{
			int x;
			for (x=0 ; x<sizeof(rateMsg)-1; x++)
				re.DrawChar (1+(x*8), 250, 128 + rateMsg[x] );
		}

		old = &cl.frames[cl.frame.deltaframe & UPDATE_MASK];
		if (old->serverframe != cl.frame.deltaframe)
		{	
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			int x;
			for (x=0 ; x<sizeof(frameMsg)-1; x++)
				re.DrawChar (1+(x*8), 266, 128 + frameMsg[x] );
		}
		
		if (cl.parse_entities - old->parse_entities > MAX_PARSE_ENTITIES-128)
		{
			int x;
			for (x=0 ; x<sizeof(parseMsg)-1; x++)
				re.DrawChar (1+(x*8), 282, 128 + parseMsg[x] );
		}

		if (!gotFrameFromServerPacket)
		{
			int x;
			for (x=0 ; x<sizeof(overflowMsg)-1; x++)
				re.DrawChar (1+(x*8), 298, 128 + overflowMsg[x] );
		}
	}

	SCR_AddDirtyPoint (scr_vrect.x, scr_vrect.y);
	SCR_AddDirtyPoint (scr_vrect.x+scr_vrect.width-1,
		scr_vrect.y+scr_vrect.height-1);

	SCR_DrawCrosshair ();
}


/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f (void)
{
	Com_Printf ("(%i %i %i) : %i\n", (int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1], (int)cl.refdef.vieworg[2], 
		(int)cl.refdef.viewangles[YAW]);
}

void OnCrossHairChange (cvar_t *self, char *old, char *new)
{
	self->modified = false;
	SCR_TouchPics();
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("gun_next", V_Gun_Next_f);
	Cmd_AddCommand ("gun_prev", V_Gun_Prev_f);
	Cmd_AddCommand ("gun_model", V_Gun_Model_f);

	Cmd_AddCommand ("viewpos", V_Viewpos_f);

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE);
	crosshair->changed = OnCrossHairChange;

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0);

	cl_stats = Cvar_Get ("cl_stats", "0", 0);
	cl_drawfps = Cvar_Get ("cl_drawfps", "0", 0);
	cl_stfu_ilkhan = Cvar_Get ("cl_stfu_ilkhan", "0", 0);
	cl_defermodels = Cvar_Get ("cl_defermodels", "1", 0);
}
