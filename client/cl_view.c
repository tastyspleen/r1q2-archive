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

extern int deferred_model_index;

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
cvar_t		*cl_drawfps_x;
cvar_t		*cl_drawfps_y;

cvar_t		*cl_stfu_ilkhan;
cvar_t		*cl_drawmaptime_x;
cvar_t		*cl_drawmaptime_y;

cvar_t		*cl_defermodels;
cvar_t		*cl_particlecount;

cvar_t		*scr_crosshair_x;
cvar_t		*scr_crosshair_y;

extern cvar_t		*scr_showturtle;

int			r_numdlights;
dlight_t	r_dlights[MAX_DLIGHTS];

int			r_numentities;
entity_t	r_entities[MAX_ENTITIES];

int			r_numparticles;
particle_t	*r_particles;//[MAX_PARTICLES];

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
void V_AddParticle (vec3_t org, unsigned color, float alpha)
{
	particle_t	*p;

	if (r_numparticles >= cl_particlecount->intvalue)
		return;

	if (color > 0xFF)
		Com_Error (ERR_DROP, "V_AddParticle: bad color %d", color);

	p = &r_particles[r_numparticles++];
	FastVectorCopy (*org, p->origin);
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
	FastVectorCopy (*org, dl->origin);
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

	r_numparticles = cl_particlecount->intvalue;
	for (i=0 ; i<r_numparticles ; i++)
	{
		d = i*0.25f;
		r = 4*((i&7)-3.5f);
		u = 4*(((i>>3)&7)-3.5f);
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
	int			i, j;
	float		f, r;
	entity_t	*ent;

	r_numentities = 32;
	memset (r_entities, 0, sizeof(r_entities));

	for (i=0 ; i<r_numentities ; i++)
	{
		ent = &r_entities[i];

		r = 64 * ( (i%4) - 1.5f );
		f = (float)(64 * (i/4) + 128);

		for (j=0 ; j<3 ; j++)
			ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;

		ent->model = cl.baseclientinfo.model;
		ent->skin = cl.baseclientinfo.skin;
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = 32;
	memset (r_dlights, 0, sizeof(r_dlights));

	for (i=0 ; i<r_numdlights ; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ( (i%4) - 1.5f );
		f = 64 * (i/4.0f) + 128;

		for (j=0 ; j<3 ; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;
		dl->color[0] = (float)(((i%6)+1) & 1);
		dl->color[1] = (float)((((i%6)+1) & 2)>>1);
		dl->color[2] = (float)((((i%6)+1) & 4)>>2);
		dl->intensity = 200;
	}
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
	int			maxclients;

	float		rotate;
	vec3_t		axis;

	if (!cl.configstrings[CS_MODELS+1][0])
		return;		// no map loaded

	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);

	// let the render dll load the map
	Q_strncpy (mapname, cl.configstrings[CS_MODELS+1] + 5, sizeof(mapname)-1);	// skip "maps/"
	mapname[strlen(mapname)-4] = 0;		// cut off ".bsp"

	Cvar_ForceSet ("$mapname", mapname);

	// register models, pics, and skins
	Com_Printf ("Map: %s\r", LOG_CLIENT, mapname); 
	SCR_UpdateScreen ();

	// clear tents - dangling model pointers
	CL_ClearTEnts ();

	re.BeginRegistration (mapname);

	Com_Printf ("                                     \r", LOG_CLIENT);

	Sys_SendKeyEvents ();

	Netchan_Transmit (&cls.netchan, 0, NULL);

	// precache status bar pics
	Com_Printf ("pics\r", LOG_CLIENT); 
	SCR_UpdateScreen ();
	SCR_TouchPics ();
	//Com_Printf ("                                     \r", LOG_CLIENT);

	Com_Printf ("models\r", LOG_CLIENT); 
	SCR_UpdateScreen ();
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

	//modelindex 1 is always world.
	//is 0 ever used?

	cl.model_clip[0] = NULL;
	cl.model_draw[0] = NULL;

	cl.model_draw[1] = re.RegisterModel (cl.configstrings[CS_MODELS+1]);

	if (cl.configstrings[CS_MODELS+1][0] == '*')
		cl.model_clip[1] = CM_InlineModel (cl.configstrings[CS_MODELS+1]);
	else
		cl.model_clip[1] = NULL;

	if (cl_defermodels->intvalue && !cl_timedemo->intvalue)
	{
		deferred_model_index = 1;
		for (i = 2; i < MAX_MODELS; i++)
		{
			cl.model_clip[i] = NULL;
			cl.model_draw[i] = NULL;
		}
	}
	else
	{
		deferred_model_index = MAX_MODELS;
		for (i = 2; i < MAX_MODELS; i++)
		{
			if (!cl.configstrings[CS_MODELS+i][0])
				continue;

			if (cl.configstrings[CS_MODELS+i][0] != '#')
			{
				cl.model_draw[i] = re.RegisterModel (cl.configstrings[CS_MODELS+i]);
				if (cl.configstrings[CS_MODELS+i][0] == '*')
					cl.model_clip[i] = CM_InlineModel (cl.configstrings[CS_MODELS+i]);
				else
					cl.model_clip[i] = NULL;
			}

			Com_Printf ("%s                         \r", LOG_CLIENT, cl.configstrings[CS_MODELS+i]); 
			//SCR_UpdateScreen ();
			Sys_SendKeyEvents ();
		}
	}

	Com_Printf ("images\r                             ", LOG_CLIENT); 
	SCR_UpdateScreen ();
	for (i=1 ; i<MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0] ; i++)
	{
		re.RegisterPic (cl.configstrings[CS_IMAGES+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	
	Com_Printf ("                                     \r", LOG_CLIENT);

	maxclients = cl.maxclients;

	Com_Printf ("clients\r", LOG_CLIENT);
	SCR_UpdateScreen ();

	//must be zeroed to flush out old model pointers
	memset (&cl.clientinfo, 0, sizeof(cl.clientinfo));

	for (i=0 ; i<maxclients ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;

		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();
		CL_ParseClientinfo (i);
	}

	Netchan_Transmit (&cls.netchan, 0, NULL);

	CL_LoadClientinfo (&cl.baseclientinfo, "");

	// set sky textures and speed
	Com_Printf ("sky             \r", LOG_CLIENT); 
	SCR_UpdateScreen ();

	rotate = (float)atof (cl.configstrings[CS_SKYROTATE]);

	if (sscanf (cl.configstrings[CS_SKYAXIS], "%f %f %f", 
		&axis[0], &axis[1], &axis[2]) != 3)
	{
		VectorClear (axis);
	}

	re.SetSky (cl.configstrings[CS_SKY], rotate, axis);
	Com_Printf ("   \r", LOG_CLIENT);

	// the renderer can now free unneeded stuff
	if (deferred_model_index == MAX_MODELS)
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
	//cl.refdef.lightstyles = 0;

	S_StopAllSounds ();

	//can't use cls.realtime - could be out of date :)
	//if (!cl_timedemo->intvalue)
		//cl.defer_rendering = (int)(Sys_Milliseconds() + (cl_defertimer->value * 1000));
	//else
		cl.defer_rendering = 0;

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
float CalcFov (float fov_x, int width, int height)
{
	static float	a;
	static float	last_fov;
	static int		lw, lh;

	//r1: only calculate if needed
	if (width != lw || height != lh || *(int*)&fov_x != *(int*)&last_fov)
	{
		float			x;

		if (fov_x < 1 || fov_x > 179)
			Com_Error (ERR_DROP, "Bad fov: %f", fov_x);

		x = width / (float)tan(fov_x/360*M_PI);

		a = (float)atan (height/x);
		a = a*360/M_PI;

		last_fov = fov_x;
		lw = width;
		lh = height;
	}

	return a;
}

//============================================================================

// gun frame debugging functions
void V_Gun_Next_f (void)
{
	gun_frame++;
	Com_Printf ("frame %i\n", LOG_CLIENT, gun_frame);
}

void V_Gun_Prev_f (void)
{
	gun_frame--;
	if (gun_frame < 0)
		gun_frame = 0;
	Com_Printf ("frame %i\n", LOG_CLIENT, gun_frame);
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
__inline void SCR_DrawCrosshair (void)
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

	re.DrawPic (scr_vrect.x + scr_crosshair_x->intvalue + ((scr_vrect.width - crosshair_width)>>1)
	, scr_vrect.y + + scr_crosshair_y->intvalue + ((scr_vrect.height - crosshair_height)>>1), crosshair_pic);
}

int spc = 0;
int fps = 0;
int frames_this_second = 0;
uint32 frames_seconds = 0;

static const char rateMsg[] = "RATEDROP";
static const char frameMsg[] = "OLDFRAME";
static const char parseMsg[] = "OLDPARSE";
static const char overflowMsg[] = "OVERFLOW";

extern int EXPORT entitycmpfnc( const entity_t *, const entity_t * );
extern int			scr_draw_loading;
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

		if (cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.5;
		}

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
		cl.refdef.vieworg[0] += 1.0f/16;
		cl.refdef.vieworg[1] += 1.0f/16;
		cl.refdef.vieworg[2] += 1.0f/16;

		cl.refdef.x = scr_vrect.x;
		cl.refdef.y = scr_vrect.y;
		cl.refdef.width = scr_vrect.width;
		cl.refdef.height = scr_vrect.height;
		cl.refdef.fov_y = CalcFov (cl.refdef.fov_x, cl.refdef.width, cl.refdef.height);
		cl.refdef.time = cl.time * 0.001f;

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

		if (cl.defer_rendering > cls.realtime)
			r_numentities = 0;

		cl.refdef.num_entities = r_numentities;
		cl.refdef.entities = r_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.playerstate.rdflags;

		// sort entities for better cache locality
        qsort( cl.refdef.entities, cl.refdef.num_entities, sizeof( cl.refdef.entities[0] ), (int (EXPORT *)(const void *, const void *))entitycmpfnc );
	}

	if (!cl.force_refdef)
		re.RenderFrame (&cl.refdef);

	if (cl_stats->intvalue)
		Com_Printf ("ent:%i  lt:%i  part:%i\n", LOG_CLIENT, r_numentities, r_numdlights, r_numparticles);
	//if ( log_stats->value && ( log_stats_file != 0 ) )
	//	fprintf( log_stats_file, "%i,%i,%i,",r_numentities, r_numdlights, r_numparticles);

	if ((unsigned)(curtime - frames_seconds) >= 1000)
	{
		spc = serverPacketCount;
		fps = frames_this_second;
		frames_this_second = 0;
		serverPacketCount = 0;
		frames_seconds = curtime;
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

	//r1: fps display
	if (cl_drawfps->intvalue)
	{
		int		x, len;
		char	buff[16];

		frames_this_second++;

		if (cl_drawfps->intvalue == 2)
		{
			fps = (int)(1.0f / cls.frametime);
		}

		len = Com_sprintf (buff, sizeof(buff), "%d", fps);
		for (x = 0; x < len; x++)
		{
			re.DrawChar (viddef.width-26+x*8+cl_drawfps_x->intvalue, viddef.height - 16 + cl_drawfps_y->intvalue, 128 + buff[x]);
		}
	}

	//r1: map timer (don't ask)
	if (cl_stfu_ilkhan->intvalue)
	{
		char buff[16];
		int x, len;
		int secs = (cl.frame.serverframe % 600) / 10;
		int mins = cl.frame.serverframe / 600;
		
		len = Com_sprintf (buff, sizeof(buff), "%d:%.2d", mins, secs);
		for (x = 0; x < len; x++)
		{
			re.DrawChar (x * 8 + cl_drawmaptime_x->intvalue, viddef.height - 8 + cl_drawmaptime_y->intvalue, 128 + buff[x]);
		}
	}

	//FIXME: incorrect use of scr_ prefix
	if (scr_showturtle->intvalue && !scr_draw_loading && cls.state == ca_active)
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

		if (noFrameFromServerPacket > 2)
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
	Com_Printf ("(%i %i %i) : %i\n", LOG_CLIENT, (int)cl.refdef.vieworg[0],
		(int)cl.refdef.vieworg[1], (int)cl.refdef.vieworg[2], 
		(int)cl.refdef.viewangles[YAW]);
}

void OnCrossHairChange (cvar_t *self, char *old, char *newValue)
{
	self->modified = false;
	SCR_TouchPics();
}

//ick
extern cparticle_t	*particles;
void CL_ClearParticles (int num);
static int num_particles;
void _particlecount_changed (cvar_t *self, char *old, char *newValue)
{
	int		count;

	//update cvar if we had to cap
	if (self->intvalue < 1024)
	{
		Cvar_Set (self->name, "1024");
		return;
	}
	else if (self->intvalue > 1048576)
	{
		Cvar_Set (self->name, "1048576");
		return;
	}

	if (particles)
	{
		CL_ClearParticles (num_particles);
		Z_Free (particles);
	}

	if (r_particles)
	{
		r_numparticles = 0;
		Z_Free (r_particles);
	}

	count = self->intvalue;

	particles = Z_TagMalloc (count * sizeof(*particles), TAGMALLOC_CL_PARTICLES);
	r_particles = Z_TagMalloc (count * sizeof(*r_particles), TAGMALLOC_CL_PARTICLES);

	//allocated uninit
	CL_ClearParticles (count);

	num_particles = count;
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

	cl_particlecount = Cvar_Get ("cl_particlecount", "16384", 0);
	cl_particlecount->changed = _particlecount_changed;
	_particlecount_changed (cl_particlecount, cl_particlecount->string, cl_particlecount->string);

	cl_testblend = Cvar_Get ("cl_testblend", "0", 0);
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0);
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0);
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0);

	cl_stats = Cvar_Get ("cl_stats", "0", 0);

	cl_drawfps = Cvar_Get ("cl_drawfps", "0", 0);
	cl_drawfps_x = Cvar_Get ("cl_drawfps_x", "0", 0);
	cl_drawfps_y = Cvar_Get ("cl_drawfps_y", "0", 0);

	cl_stfu_ilkhan = Cvar_Get ("cl_drawmaptime", "0", 0);
	cl_drawmaptime_x = Cvar_Get ("cl_drawmaptime_x", "0", 0);
	cl_drawmaptime_y = Cvar_Get ("cl_drawmaptime_y", "0", 0);

	cl_defermodels = Cvar_Get ("cl_defermodels", "1", 0);

	scr_crosshair_x = Cvar_Get ("scr_crosshair_x", "0", 0);
	scr_crosshair_y = Cvar_Get ("scr_crosshair_y", "0", 0);
}
