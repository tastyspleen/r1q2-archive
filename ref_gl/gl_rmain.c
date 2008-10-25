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
// r_main.c
#include "gl_local.h"

#ifndef WIN32
#define __stdcall
#endif

//long (*Q_ftol)(float f);

void R_Clear (void);

viddef_t	vid;

refimport_t		ri;
refimportnew_t	rx;

unsigned int GL_TEXTURE0, GL_TEXTURE1;

model_t		*r_worldmodel;

double		gldepthmin, gldepthmax;

double		vid_scaled_width, vid_scaled_height;

glconfig_t gl_config;
glstate_t  gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_particletexture;	// little dot for particles

entity_t	*currententity;
model_t		*currentmodel;

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

float		v_blend[4];			// final blending color

void GL_Strings_f( void );

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

//cvar_t	*gl_nosubimage;
cvar_t	*gl_allow_software;

cvar_t	*gl_vertex_arrays;

cvar_t	*gl_particle_min_size;
cvar_t	*gl_particle_max_size;
cvar_t	*gl_particle_size;
cvar_t	*gl_particle_att_a;
cvar_t	*gl_particle_att_b;
cvar_t	*gl_particle_att_c;

//cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_palettedtexture;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_pointparameters;
//cvar_t	*gl_ext_compiled_vertex_array;

//r1ch: my extensions
//cvar_t	*gl_ext_generate_mipmap;
cvar_t	*gl_ext_point_sprite;
cvar_t	*gl_ext_texture_filter_anisotropic;
cvar_t	*gl_ext_texture_non_power_of_two;
cvar_t	*gl_ext_max_anisotropy;
cvar_t	*gl_ext_nv_multisample_filter_hint;
cvar_t	*gl_ext_occlusion_query;

cvar_t	*gl_colorbits;
cvar_t	*gl_alphabits;
cvar_t	*gl_depthbits;
cvar_t	*gl_stencilbits;

cvar_t	*gl_ext_multisample;
cvar_t	*gl_ext_samples;

cvar_t	*gl_zfar;
cvar_t	*gl_hudscale;

cvar_t	*cl_version;
cvar_t	*gl_r1gl_test;
cvar_t	*gl_doublelight_entities;
cvar_t	*gl_noscrap;
cvar_t	*gl_overbrights;
cvar_t	*gl_linear_mipmaps;

cvar_t	*vid_gamma_pics;

cvar_t	*gl_forcewidth;
cvar_t	*gl_forceheight;

cvar_t	*vid_topmost;

//cvar_t	*gl_log;
cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
//cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
//cvar_t  *gl_monolightmap;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_skymip;
cvar_t	*gl_showtris;
cvar_t	*gl_ztrick;
cvar_t	*gl_finish;
cvar_t	*gl_flush;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_flashblend;
//cvar_t	*gl_playermip;
//cvar_t  *gl_saturatelighting;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturealphamode;
cvar_t	*gl_texturesolidmode;
cvar_t	*gl_lockpvs;
cvar_t	*gl_jpg_quality;
cvar_t	*gl_coloredlightmaps;

//cvar_t	*gl_3dlabs_broken;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_ref;
cvar_t	*vid_forcedrefresh;
cvar_t	*vid_optimalrefresh;
cvar_t	*vid_nowgl;
cvar_t	*vid_restore_on_switch;

cvar_t	*gl_texture_formats;
cvar_t	*gl_pic_formats;

cvar_t	*gl_dlight_falloff;
cvar_t	*gl_alphaskins;
cvar_t	*gl_defertext;

cvar_t	*gl_pic_scale;

//cvar_t	*con_alpha;

vec4_t	colorWhite = {1,1,1,1};

qboolean load_png_pics = true;
qboolean load_tga_pics = true;
qboolean load_jpg_pics = true;

qboolean load_png_wals = true;
qboolean load_tga_wals = true;
qboolean load_jpg_wals = true;

extern cvar_t		*gl_contrast;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	if (FLOAT_NE_ZERO(r_nocull->value))
		return false;

	for (i=0 ; i<4 ; i++)
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}


void R_RotateForEntity (entity_t *e)
{
    qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    qglRotatef (e->angles[1],  0, 0, 1);
    qglRotatef (-e->angles[0],  0, 1, 0);
    qglRotatef (-e->angles[2],  1, 0, 0);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	float alpha = 1.0F;
	vec3_t	point;
	dsprframe_t	*frame;
	float		*up, *right;
	dsprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (dsprite_t *)currentmodel->extradata;

#if 0
	if (e->frame < 0 || e->frame >= psprite->numframes)
	{
		ri.Con_Printf (PRINT_ALL, "no such sprite frame %i\n", e->frame);
		e->frame = 0;
	}
#endif
	e->frame %= psprite->numframes;

	frame = &psprite->frames[e->frame];

#if 0
	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
	vec3_t		v_forward, v_right, v_up;

	AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
#endif
	{	// normal sprite
		up = vup;
		right = vright;
	}

	if ( e->flags & RF_TRANSLUCENT )
		alpha = e->alpha;

	if ( alpha != 1.0F )
		qglEnable( GL_BLEND );

	qglColor4f( 1, 1, 1, alpha );

    GL_Bind(currentmodel->skins[e->frame]->texnum);

	GL_TexEnv( GL_MODULATE );

	if ( alpha == 1.0 )
		qglEnable (GL_ALPHA_TEST);
	else
		qglDisable( GL_ALPHA_TEST );

	qglBegin (GL_QUADS);

	qglTexCoord2f (0, 1);
	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, -frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->height - frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 1);
	VectorMA (e->origin, -frame->origin_y, up, point);
	VectorMA (point, frame->width - frame->origin_x, right, point);
	qglVertex3fv (point);
	
	qglEnd ();

	qglDisable (GL_ALPHA_TEST);
	GL_TexEnv( GL_REPLACE );

	if ( alpha != 1.0F )
		qglDisable( GL_BLEND );

	qglColor4fv(colorWhite);
}

//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (void)
{
	vec3_t	shadelight;
	int		i;

	if ( currententity->flags & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	else
		R_LightPoint (currententity->origin, shadelight);

    qglPushMatrix ();
	R_RotateForEntity (currententity);

	qglDisable (GL_TEXTURE_2D);
	qglColor3fv (shadelight);

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, -16);
	for (i=0 ; i<=4 ; i++)
		qglVertex3f (16*(float)cos(i*M_PI_DIV_2), 16*(float)sin(i*M_PI_DIV_2), 0);
	qglEnd ();

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, 16);
	for (i=4 ; i>=0 ; i--)
		qglVertex3f (16*(float)cos(i*M_PI_DIV_2), 16*(float)sin(i*M_PI_DIV_2), 0);
	qglEnd ();

	qglColor3f (1,1,1);
	qglPopMatrix ();
	qglEnable (GL_TEXTURE_2D);
}

int visibleBits[MAX_ENTITIES];


void R_Occlusion_Results (void)
{
	int		i, visible;
	entity_t	*ent;
	//int		numOccluded = 0;

	// now we read back
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		int	available;

		ent = &r_newrefdef.entities[i];

		if (!ent->model || ent->model->type == mod_brush)
		{
			visibleBits[i] = 500;
			continue;
		}

		if (visibleBits[i] > 1)
		{
			visibleBits[i]--;
			continue;
		}

		qglGetQueryObjectivARB (gl_config.r1gl_Queries[i], GL_QUERY_RESULT_AVAILABLE_ARB, &available);
		if (!available)
		{
			if (gl_ext_occlusion_query->value == 2.0f)
				i--;
			else
				visibleBits[i] = 25;

			continue;
		}

		// get the object and store it in the occlusion bits for the ent
		qglGetQueryObjectivARB (gl_config.r1gl_Queries[i], GL_QUERY_RESULT, &visible);

		if (!visible)
		{
			//ri.Con_Printf (PRINT_ALL, "Occluded %d, %s\n", i, ent->model->name);
			visibleBits[i] = 0;
		}
		else
			visibleBits[i] = 25;
	}
}

void R_Occlusion_Run (void)
{
	int		i;
	entity_t	*ent;
	float	mins[3];
	float	maxs[3];

	static const byte boxindexes[] =
	{
	0, 1, 2, 3,
	4, 5, 1, 0,
	3, 2, 6, 7,
	5, 4, 7, 6,
	1, 5, 6, 2,
	4, 0, 3, 7
	};

	float	boxverts[24];

	if (!r_newrefdef.num_entities)
		return;

	// disable texturing
	qglDisable (GL_TEXTURE_2D);

	// because we don;t know the orientation of the bbox in advance...
	qglDisable (GL_CULL_FACE);

	// disable framebuffer and depthbuffer writes
	qglColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	qglDepthMask (GL_FALSE);

	qglEnableClientState (GL_VERTEX_ARRAY);
	qglVertexPointer (3, GL_FLOAT, 0, boxverts);

	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		ent = &r_newrefdef.entities[i];

		if (!ent->model || ent->model->type == mod_brush)
			continue;

		if (visibleBits[i] > 1)
			continue;

		// get mins and maxs points
		VectorAdd (ent->origin, ent->model->mins, mins);
		VectorAdd (ent->origin, ent->model->maxs, maxs);

		// CPU grunt to the rescue!!!
		boxverts[0] = boxverts[9] = boxverts[12] = boxverts[21] = mins[0];
		boxverts[3] = boxverts[6] = boxverts[15] = boxverts[18] = maxs[0];
		boxverts[1] = boxverts[4] = boxverts[13] = boxverts[16] = maxs[1];
		boxverts[7] = boxverts[10] = boxverts[19] = boxverts[22] = mins[1];
		boxverts[2] = boxverts[5] = boxverts[8] = boxverts[11] = maxs[2];
		boxverts[14] = boxverts[17] = boxverts[20] = boxverts[23] = mins[2];

		// begin the occlusion query
		qglBeginQueryARB (GL_SAMPLES_PASSED, gl_config.r1gl_Queries[i]);

		// draw as indexed varray
		qglDrawElements (GL_QUADS, 24, GL_UNSIGNED_BYTE, boxindexes);

		// end the query
		// don't read back immediately so that we give the query time to be ready
		qglEndQueryARB (GL_SAMPLES_PASSED);
	}

	qglDisableClientState (GL_VERTEX_ARRAY);

	// restore basic state
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_CULL_FACE);

	// enable framebuffer and depthbuffer writes
	qglColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	qglDepthMask (GL_TRUE);

	// some implementations don't reset the primary colour properly after restoring the colormask
	qglColor4f  (1, 1, 1, 1);
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (FLOAT_EQ_ZERO(r_drawentities->value))
		return;

	if (gl_config.r1gl_QueryBits)
		R_Occlusion_Results ();

	// draw non-transparent first
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		if (gl_config.r1gl_QueryBits && !visibleBits[i])
			continue;

		currententity = &r_newrefdef.entities[i];

		if (currententity->flags & RF_TRANSLUCENT || (FLOAT_NE_ZERO(gl_alphaskins->value) && currententity->skin && currententity->skin->has_alpha))
			continue;	// solid

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					R_DrawAliasModel (currententity);
					break;
				case mod_brush:
					R_DrawBrushModel (currententity);
					break;
				case mod_sprite:
					R_DrawSpriteModel (currententity);
					break;
				default:
					ri.Sys_Error (ERR_DROP, "Bad modeltype %d on %s", currentmodel->type, currentmodel->name);
					break;
			}
		}
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...
	qglDepthMask (0);		// no z writes
	for (i=0 ; i<r_newrefdef.num_entities ; i++)
	{
		currententity = &r_newrefdef.entities[i];
		if (!(currententity->flags & RF_TRANSLUCENT || (FLOAT_NE_ZERO(gl_alphaskins->value) && currententity->skin && currententity->skin->has_alpha)))
			continue;	// solid

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type)
			{
			case mod_alias:
				R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
			default:
				ri.Sys_Error (ERR_DROP, "Bad modeltype %d on %s", currentmodel->type, currentmodel->name);
				break;
			}
		}
	}
	qglDepthMask (1);		// back to writing

}

/*
** GL_DrawParticles
**
*/
void GL_DrawParticles( int num_particles, const particle_t particles[])
{
	const particle_t *p;
	int				i;
	vec3_t			up, right;
	float			scale;
	//byte			color[4];
	vec4_t			colorf;

    GL_Bind(r_particletexture->texnum);
	qglDepthMask( GL_FALSE );		// no z buffering
	qglEnable( GL_BLEND );
	GL_TexEnv( GL_MODULATE );
	qglBegin( GL_TRIANGLES );

	VectorScale (vup, 1.5f, up);
	VectorScale (vright, 1.5f, right);

	for ( p = particles, i=0 ; i < num_particles ; i++,p++)
	{
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vpn[0] + 
			    ( p->origin[1] - r_origin[1] ) * vpn[1] +
			    ( p->origin[2] - r_origin[2] ) * vpn[2];

		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004f;

		//*(int *)color = colortable[p->color];
		//color[3] = (byte)Q_ftol(p->alpha*255);

		FastVectorCopy (d_8to24float[p->color], colorf);
		colorf[3] = p->alpha;

		qglColor4fv( colorf );

		qglTexCoord2f( 0.0625f, 0.0625f );
		qglVertex3fv( p->origin );

		qglTexCoord2f( 1.0625f, 0.0625f );
		qglVertex3f( p->origin[0] + up[0]*scale, 
			         p->origin[1] + up[1]*scale, 
					 p->origin[2] + up[2]*scale);

		qglTexCoord2f( 0.0625f, 1.0625f );
		qglVertex3f( p->origin[0] + right[0]*scale, 
			         p->origin[1] + right[1]*scale, 
					 p->origin[2] + right[2]*scale);
	}

	qglEnd ();
	qglDisable( GL_BLEND );
	qglColor4fv(colorWhite);
	qglDepthMask( 1 );		// back to normal Z buffering
	GL_TexEnv( GL_REPLACE );
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if (gl_config.r1gl_GL_ARB_point_sprite && FLOAT_NE_ZERO(gl_ext_point_sprite->value))
	{
		const float quadratic[] =  { 1.0f, 0.0f, 0.0005f };

		GL_Bind (r_particletexture->texnum);

		GL_TexEnv( GL_MODULATE );
		qglDepthMask( GL_FALSE );
		//qglDisable( GL_TEXTURE_2D );

		qglEnable( GL_BLEND );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		qglPointParameterfvARB( GL_POINT_DISTANCE_ATTENUATION_ARB, quadratic );

		qglPointSize( gl_particle_size->value );

		// The alpha of a point is calculated to allow the fading of points 
		// instead of shrinking them past a defined threshold size. The threshold 
		// is defined by GL_POINT_FADE_THRESHOLD_SIZE_ARB and is not clamped to 
		// the minimum and maximum point sizes.
		qglPointParameterfARB( GL_POINT_FADE_THRESHOLD_SIZE_ARB, gl_particle_max_size->value );

		qglPointParameterfARB( GL_POINT_SIZE_MIN_ARB, gl_particle_min_size->value );
		qglPointParameterfARB( GL_POINT_SIZE_MAX_ARB, gl_particle_max_size->value );

		// Specify point sprite texture coordinate replacement mode for each texture unit
		qglTexEnvf( GL_POINT_SPRITE_ARB, GL_COORD_REPLACE_ARB, GL_TRUE );

		//
		// Render point sprites...
		//

		qglEnable( GL_POINT_SPRITE_ARB );
		qglBegin( GL_POINTS );
		{
			const particle_t *p;
			int i;
			//unsigned char color[4];
			vec4_t	colorf;

			for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
			{
				//*(int *)color = d_8to24table[p->color];
				//color[3] = (byte)Q_ftol(p->alpha*255);

				//qglColor4ubv( color );
				
				FastVectorCopy (d_8to24float[p->color], colorf);
				colorf[3] = p->alpha;
				qglColor4fv( colorf );
				
				qglVertex3fv( p->origin );
			}
		}
		qglEnd();

		qglDisable( GL_POINT_SPRITE_ARB );
		qglDisable( GL_BLEND );
		qglColor4fv(colorWhite);
		qglDepthMask( GL_TRUE );
		qglEnable( GL_TEXTURE_2D );
		qglDepthMask( 1 );		// back to normal Z buffering
		GL_TexEnv( GL_REPLACE );
	}
	else if ( qglPointParameterfEXT && FLOAT_NE_ZERO(gl_ext_pointparameters->value))
	{
		int i;
		vec4_t			colorf;
		//unsigned char color[4];
		const particle_t *p;

		qglDepthMask( GL_FALSE );
		qglEnable( GL_BLEND );
		qglDisable( GL_TEXTURE_2D );

		qglPointSize( gl_particle_size->value );

		qglBegin( GL_POINTS );

		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			//*(vec4_t **)&colorf = *(vec4_t *)&d_8to24float[p->color];
			//memcpy (colorf, d_8to24float[p->color], sizeof(colorf));
			FastVectorCopy (d_8to24float[p->color], colorf);
			colorf[3] = p->alpha;

			//*(int *)color = d_8to24table[p->color];
			//color[3] = (byte)Q_ftol (p->alpha * 255);
			//qglColor4ubv( color );
			qglColor4fv( colorf );
			qglVertex3fv( p->origin );
		}

		qglEnd();

		qglDisable( GL_BLEND );
		qglColor4fv(colorWhite);
		qglDepthMask( GL_TRUE );
		qglEnable( GL_TEXTURE_2D );

	}
	else
	{
		GL_DrawParticles( r_newrefdef.num_particles, r_newrefdef.particles );
	}
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (FLOAT_EQ_ZERO(gl_polyblend->value))
		return;

	if (FLOAT_EQ_ZERO(v_blend[3]))
		return;

	qglDisable (GL_ALPHA_TEST);
	qglEnable (GL_BLEND);
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_TEXTURE_2D);

    qglLoadIdentity ();

	// FIXME: get rid of these
    qglRotatef (-90,  1, 0, 0);	    // put Z going up
    qglRotatef (90,  0, 0, 1);	    // put Z going up

	qglColor4fv (v_blend);

	qglBegin (GL_QUADS);

	qglVertex3f (10, 100, 100);
	qglVertex3f (10, -100, 100);
	qglVertex3f (10, -100, -100);
	qglVertex3f (10, 100, -100);
	qglEnd ();

	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglEnable (GL_ALPHA_TEST);

	qglColor4fv(colorWhite);
}

//=======================================================================

int SignbitsForPlane (cplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (FLOAT_LT_ZERO(out->normal[j]))
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

#if 0
	/*
	** this code is wrong, since it presume a 90 degree FOV both in the
	** horizontal and vertical plane
	*/
	// front side is visible
	VectorAdd (vpn, vright, frustum[0].normal);
	VectorSubtract (vpn, vright, frustum[1].normal);
	VectorAdd (vpn, vup, frustum[2].normal);
	VectorSubtract (vpn, vup, frustum[3].normal);

	// we theoretically don't need to normalize these vectors, but I do it
	// anyway so that debugging is a little easier
	VectorNormalize( frustum[0].normal );
	VectorNormalize( frustum[1].normal );
	VectorNormalize( frustum[2].normal );
	VectorNormalize( frustum[3].normal );
#else
	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2 ) );
#endif

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	mleaf_t	*leaf;

	r_framecount++;

// build the transformation matrix for the given view angles
	FastVectorCopy (r_newrefdef.vieworg, r_origin);

	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

// current viewcluster
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			FastVectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			FastVectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}

	v_blend[0] = r_newrefdef.blend[0];
	v_blend[1] = r_newrefdef.blend[1];
	v_blend[2] = r_newrefdef.blend[2];
	v_blend[3] = r_newrefdef.blend[3];

	c_brush_polys = 0;
	c_alias_polys = 0;

	// clear out the portion of the screen that the NOWORLDMODEL defines
	/*if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
	{
		qglEnable( GL_SCISSOR_TEST );
		qglClearColor( 0.3f, 0.3f, 0.3f, 1 );
		
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		
		qglClearColor( 1, 0, 0.5f, 0.5f );

		qglDisable( GL_SCISSOR_TEST );
	}*/
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		qglEnable(GL_SCISSOR_TEST);
		qglClearColor(0.3f, 0.3f, 0.3f, 1);
		qglScissor(r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width,
			   r_newrefdef.height);
		qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		qglClearColor(0, 0, 0, 1);
		qglDisable(GL_SCISSOR_TEST);
	}
}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * M_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

#ifdef STERO_SUPPORT
   xmin += -( 2 * gl_state.camera_separation ) / zNear;
   xmax += -( 2 * gl_state.camera_separation ) / zNear;
#endif

   qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
//	float	yfov;
	int		x, x2, y2, y, w, h;

	//
	// set up viewport
	//
	x = (int)floor(r_newrefdef.x * vid.width / vid.width);
	x2 = (int)ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = (int)floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = (int)ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	qglViewport (x, y2, w, h);

	//
	// set up projection matrix
	//
    screenaspect = (float)r_newrefdef.width/r_newrefdef.height;
//	yfov = 2*atan((float)r_newrefdef.height/r_newrefdef.width)*180/M_PI;
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
    MYgluPerspective (r_newrefdef.fov_y,  screenaspect,  4,  gl_zfar->value);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

    qglRotatef (-90,  1, 0, 0);	    // put Z going up
    qglRotatef (90,  0, 0, 1);	    // put Z going up
    qglRotatef (-r_newrefdef.viewangles[2],  1, 0, 0);
    qglRotatef (-r_newrefdef.viewangles[0],  0, 1, 0);
    qglRotatef (-r_newrefdef.viewangles[1],  0, 0, 1);
    qglTranslatef (-r_newrefdef.vieworg[0],  -r_newrefdef.vieworg[1],  -r_newrefdef.vieworg[2]);

//	if ( gl_state.camera_separation != 0 && gl_state.stereo_enabled )
//		qglTranslatef ( gl_state.camera_separation, 0, 0 );

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (FLOAT_NE_ZERO(gl_cull->value))
		qglEnable(GL_CULL_FACE);
	else
		qglDisable(GL_CULL_FACE);

	//qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	//qglEnable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/

float ref_frand(void)
{
	return (((rand()&32767)) * .0000305185094759971922971282082583086642f);
}

void R_Clear (void)
{
	if (FLOAT_NE_ZERO(gl_ztrick->value) && r_worldmodel != NULL)
	{
		static int trickframe;

		if (FLOAT_NE_ZERO(gl_clear->value))
		{
			if (gl_clear->value == 2)
			{
				qglClearColor (ref_frand(), ref_frand(), ref_frand(), 1.0);
				GL_CheckForError ();
			}
			qglClear (GL_COLOR_BUFFER_BIT);
			GL_CheckForError ();
		}

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
			GL_CheckForError ();
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
			GL_CheckForError ();
		}
	}
	else
	{
		if (FLOAT_NE_ZERO(gl_clear->value))
		{
			if (gl_clear->value == 2)
			{
				qglClearColor (ref_frand(), ref_frand(), ref_frand(), 1.0);
				GL_CheckForError ();
			}

			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			GL_CheckForError ();
		}
		else
		{
			qglClear (GL_DEPTH_BUFFER_BIT);
			GL_CheckForError ();
		}

		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
		GL_CheckForError ();
	}

	qglDepthRange (gldepthmin, gldepthmax);
	GL_CheckForError ();
}

/*void R_Flash( void )
{
	R_PolyBlend ();
}*/

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	if (FLOAT_NE_ZERO(r_norefresh->value))
		return;

	r_newrefdef = *fd;

	if (FLOAT_NE_ZERO(gl_hudscale->value))
	{
		r_newrefdef.width = (int)(r_newrefdef.width * gl_hudscale->value);
		r_newrefdef.height = (int)(r_newrefdef.height * gl_hudscale->value);
		r_newrefdef.x = (int)(r_newrefdef.x * gl_hudscale->value);
		r_newrefdef.y = (int)(r_newrefdef.y * gl_hudscale->value);
	}

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		ri.Sys_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	//if (FLOAT_NE_ZERO(r_speeds->value))
	//{
	c_brush_polys = 0;
	c_alias_polys = 0;
	//}

	R_PushDlights ();

	if (FLOAT_NE_ZERO(gl_flush->value))
		qglFlush ();

	if (FLOAT_NE_ZERO(gl_finish->value))
		qglFinish ();

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	if (gl_config.r1gl_QueryBits)
		R_Occlusion_Run ();

	R_DrawWorld ();

	R_DrawEntitiesOnList ();

	R_RenderDlights ();

	R_DrawParticles ();

	R_DrawAlphaSurfaces ();

	R_PolyBlend();
	
	if (FLOAT_NE_ZERO(r_speeds->value))
	{
		ri.Con_Printf (PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
			c_brush_polys, 
			c_alias_polys, 
			c_visible_textures, 
			c_visible_lightmaps); 
	}
}


void	R_SetGL2D (void)
{
	// set 2D virtual screen size
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	//qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	qglOrtho(0, vid_scaled_width, vid_scaled_height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	//GLPROFqglDisable (GL_BLEND);
	qglEnable (GL_ALPHA_TEST);
	qglColor4fv(colorWhite);
}

#ifdef STEREO_SUPPORT
static void GL_DrawColoredStereoLinePair( float r, float g, float b, float y )
{
	qglColor3f( r, g, b );
	qglVertex2f( 0, y );
	qglVertex2f( vid.width, y );
	qglColor3f( 0, 0, 0 );
	qglVertex2f( 0, y + 1 );
	qglVertex2f( vid.width, y + 1 );
}

static void GL_DrawStereoPattern( void )
{
	int i;

	if ( !( gl_config.renderer & GL_RENDERER_INTERGRAPH ) )
		return;

	if ( !gl_state.stereo_enabled )
		return;

	R_SetGL2D();

	qglDrawBuffer( GL_BACK_LEFT );

	for ( i = 0; i < 20; i++ )
	{
		qglBegin( GL_LINES );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 0 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 2 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 4 );
			GL_DrawColoredStereoLinePair( 1, 0, 0, 6 );
			GL_DrawColoredStereoLinePair( 0, 1, 0, 8 );
			GL_DrawColoredStereoLinePair( 1, 1, 0, 10);
			GL_DrawColoredStereoLinePair( 1, 1, 0, 12);
			GL_DrawColoredStereoLinePair( 0, 1, 0, 14);
		qglEnd();
		
		GLimp_EndFrame();
	}
}
#endif


/*
====================
R_SetLightLevel

====================
*/
void R_SetLightLevel (void)
{
	vec3_t		shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint (r_newrefdef.vieworg, shadelight);

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
			r_lightlevel->value = 150*shadelight[0];
		else
			r_lightlevel->value = 150*shadelight[2];
	}
	else
	{
		if (shadelight[1] > shadelight[2])
			r_lightlevel->value = 150*shadelight[1];
		else
			r_lightlevel->value = 150*shadelight[2];
	}

}

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void EXPORT R_RenderFrame (refdef_t *fd)
{
	R_RenderView( fd );
	R_SetLightLevel ();
	R_SetGL2D ();
}

void Cmd_HashStats_f (void);
void R_Register( void )
{
	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0);
	r_novis = ri.Cvar_Get ("r_novis", "0", 0);
	r_nocull = ri.Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = ri.Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0);

	r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", CVAR_NOSET);

	//gl_nosubimage = ri.Cvar_Get( "gl_nosubimage", "0", 0 );
	gl_allow_software = ri.Cvar_Get( "gl_allow_software", "0", 0 );

	gl_particle_min_size = ri.Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE );
	gl_particle_max_size = ri.Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE );
	gl_particle_size = ri.Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE );
	gl_particle_att_a = ri.Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE );
	gl_particle_att_b = ri.Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE );
	gl_particle_att_c = ri.Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE );

	gl_modulate = ri.Cvar_Get ("gl_modulate", "2", CVAR_ARCHIVE );
	//gl_log = ri.Cvar_Get( "gl_log", "0", 0 );
	gl_bitdepth = ri.Cvar_Get( "gl_bitdepth", "0", 0 );
	gl_mode = ri.Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE );
	//gl_lightmap = ri.Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE );
	gl_dynamic = ri.Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = ri.Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = ri.Cvar_Get ("gl_round_down", "0", 0);
	gl_picmip = ri.Cvar_Get ("gl_picmip", "0", 0);
	gl_skymip = ri.Cvar_Get ("gl_skymip", "0", 0);
	gl_showtris = ri.Cvar_Get ("gl_showtris", "0", 0);
	gl_ztrick = ri.Cvar_Get ("gl_ztrick", "0", 0);
	gl_finish = ri.Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_flush = ri.Cvar_Get ("gl_flush", "0", CVAR_ARCHIVE);
	gl_clear = ri.Cvar_Get ("gl_clear", "0", 0);
	gl_cull = ri.Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = ri.Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = ri.Cvar_Get ("gl_flashblend", "0", 0);
	//gl_playermip = ri.Cvar_Get ("gl_playermip", "0", 0);
	//gl_monolightmap = ri.Cvar_Get( "gl_monolightmap", "0", 0 );
	gl_driver = ri.Cvar_Get( "gl_driver", "opengl32", CVAR_ARCHIVE );
	gl_texturemode = ri.Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );
	gl_texturealphamode = ri.Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE );
	gl_texturesolidmode = ri.Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE );
	gl_lockpvs = ri.Cvar_Get( "gl_lockpvs", "0", 0 );

	gl_vertex_arrays = ri.Cvar_Get( "gl_vertex_arrays", "1", CVAR_ARCHIVE );

	//gl_ext_swapinterval = ri.Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE );
	//gl_ext_palettedtexture = ri.Cvar_Get( "gl_ext_palettedtexture", "0", CVAR_ARCHIVE );
	gl_ext_multitexture = ri.Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE );
	
	//note, pointparams moved to init to handle defaults
	//gl_ext_compiled_vertex_array = ri.Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE );

	//r1ch: my extensions
	//gl_ext_generate_mipmap = ri.Cvar_Get ("gl_ext_generate_mipmap", "0", 0);
	gl_ext_point_sprite = ri.Cvar_Get ("gl_ext_point_sprite", "0", 0);
	gl_ext_texture_filter_anisotropic = ri.Cvar_Get ("gl_ext_texture_filter_anisotropic", "0", 0);
	gl_ext_texture_non_power_of_two = ri.Cvar_Get ("gl_ext_texture_non_power_of_two", "0", 0);
	gl_ext_max_anisotropy = ri.Cvar_Get ("gl_ext_max_anisotropy", "2", 0);
	gl_ext_occlusion_query = ri.Cvar_Get ("gl_ext_occlusion_query", "0", 0);
	
	gl_ext_nv_multisample_filter_hint = ri.Cvar_Get ("gl_ext_nv_multisample_filter_hint", "fastest", 0);

	gl_colorbits = ri.Cvar_Get ("gl_colorbits", "0", 0);
	gl_stencilbits = ri.Cvar_Get ("gl_stencilbits", "", 0);
	gl_alphabits = ri.Cvar_Get ("gl_alphabits", "", 0);
	gl_depthbits = ri.Cvar_Get ("gl_depthbits", "", 0);

	gl_ext_multisample = ri.Cvar_Get ("gl_ext_multisample", "0", 0);
	gl_ext_samples = ri.Cvar_Get ("gl_ext_samples", "2", 0);
	
	gl_zfar = ri.Cvar_Get ("gl_zfar", "8192", 0);
	gl_hudscale = ri.Cvar_Get ("gl_hudscale", "1", 0);

	cl_version = ri.Cvar_Get ("cl_version", REF_VERSION, CVAR_NOSET); 
	
	gl_r1gl_test = ri.Cvar_Get ("gl_r1gl_test", "0", 0);
	gl_doublelight_entities = ri.Cvar_Get ("gl_doublelight_entities", "1", 0);
	gl_noscrap = ri.Cvar_Get ("gl_noscrap", "1", 0);
	gl_overbrights = ri.Cvar_Get ("gl_overbrights", "0", 0);
	gl_linear_mipmaps = ri.Cvar_Get ("gl_linear_mipmaps", "0", 0);

	vid_forcedrefresh = ri.Cvar_Get ("vid_forcedrefresh", "0", 0);
	vid_optimalrefresh = ri.Cvar_Get ("vid_optimalrefresh", "0", 0);
	vid_gamma_pics = ri.Cvar_Get ("vid_gamma_pics", "0", 0);
	vid_nowgl = ri.Cvar_Get ("vid_nowgl", "0", 0);
	vid_restore_on_switch = ri.Cvar_Get ("vid_flip_on_switch", "0", 0);

	gl_forcewidth = ri.Cvar_Get ("vid_forcewidth", "0", 0);
	gl_forceheight = ri.Cvar_Get ("vid_forceheight", "0", 0);

	vid_topmost = ri.Cvar_Get ("vid_topmost", "0", 0);

	gl_pic_scale = ri.Cvar_Get ("gl_pic_scale", "1", 0);
	//r1ch end my shit

	gl_drawbuffer = ri.Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = ri.Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE );

	//gl_saturatelighting = ri.Cvar_Get( "gl_saturatelighting", "0", 0 );

	gl_jpg_quality = ri.Cvar_Get ("gl_jpg_quality", "90", 0);
	gl_coloredlightmaps = ri.Cvar_Get ("gl_coloredlightmaps", "1", 0);
	usingmodifiedlightmaps = (gl_coloredlightmaps->value != 1.0f);

	//gl_3dlabs_broken = ri.Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE );

	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );
	vid_ref = ri.Cvar_Get( "vid_ref", "r1gl", CVAR_ARCHIVE );

	gl_texture_formats = ri.Cvar_Get ("gl_texture_formats", "png jpg tga", 0);
	gl_pic_formats = ri.Cvar_Get ("gl_pic_formats", "png jpg tga", 0);

	load_png_wals = strstr (gl_texture_formats->string, "png") ? true : false;
	load_jpg_wals = strstr (gl_texture_formats->string, "jpg") ? true : false;
	load_tga_wals = strstr (gl_texture_formats->string, "tga") ? true : false;

	load_png_pics = strstr (gl_pic_formats->string, "png") ? true : false;
	load_jpg_pics = strstr (gl_pic_formats->string, "jpg") ? true : false;
	load_tga_pics = strstr (gl_pic_formats->string, "tga") ? true : false;

	gl_dlight_falloff = ri.Cvar_Get ("gl_dlight_falloff", "0", 0);
	gl_alphaskins = ri.Cvar_Get ("gl_alphaskins", "0", 0);
	gl_defertext = ri.Cvar_Get ("gl_defertext", "0", 0);
	defer_drawing = (int)gl_defertext->value;

	//con_alpha = ri.Cvar_Get ("con_alpha", "1.0", 0);

	ri.Cmd_AddCommand( "imagelist", GL_ImageList_f );
	ri.Cmd_AddCommand( "screenshot", GL_ScreenShot_f );
	ri.Cmd_AddCommand( "modellist", Mod_Modellist_f );
	ri.Cmd_AddCommand( "gl_strings", GL_Strings_f );
	ri.Cmd_AddCommand( "hash_stats", Cmd_HashStats_f );
	

#ifdef R1GL_RELEASE
	ri.Cmd_AddCommand ("r1gl_version", GL_Version_f);
#endif
}

/*
==================
R_SetMode
==================
*/
int R_SetMode (void)
{
	int err;
	qboolean fullscreen;

	fullscreen = FLOAT_EQ_ZERO(vid_fullscreen->value) ? false : true;

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	if (FLOAT_NE_ZERO(gl_forcewidth->value))
		vid.width = (int)gl_forcewidth->value;

	if (FLOAT_NE_ZERO(gl_forceheight->value))
		vid.height = (int)gl_forceheight->value;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, Q_ftol(gl_mode->value), fullscreen ) ) == VID_ERR_NONE )
	{
		gl_state.prev_mode = Q_ftol(gl_mode->value);
	}
	else
	{
		if ( err & VID_ERR_RETRY_QGL)
		{
			return err;
		}
		else if ( err & VID_ERR_FULLSCREEN_FAILED )
		{
			ri.Cvar_SetValue( "vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, Q_ftol(gl_mode->value), false ) ) == VID_ERR_NONE )
				return VID_ERR_NONE;
		}
		else if ( err & VID_ERR_FAIL )
		{
			ri.Cvar_SetValue( "gl_mode", (float)gl_state.prev_mode );
			gl_mode->modified = false;
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != VID_ERR_NONE )
		{
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n" );
			return VID_ERR_FAIL;
		}
	}
	return VID_ERR_NONE;
}

/*
===============
R_Init
===============
*/
int EXPORT R_Init( void *hinstance, void *hWnd )
{	
	char renderer_buffer[1000];
	char vendor_buffer[1000];
	int		err;
	int		j;
	extern float r_turbsin[256];

	for ( j = 0; j < 256; j++ )
	{
		r_turbsin[j] *= 0.5;
	}

	ri.Cmd_ExecuteText (EXEC_NOW, "exec r1gl.cfg\n");

	ri.Con_Printf (PRINT_ALL, "ref_gl version: "REF_VERSION"\n");

	ri.Con_Printf (PRINT_DEVELOPER, "Draw_GetPalette()\n");
	Draw_GetPalette ();

	ri.Con_Printf (PRINT_DEVELOPER, "R_Register()\n");
	R_Register();

	gl_overbrights->modified = false;

retryQGL:

	// initialize our QGL dynamic bindings
	ri.Con_Printf (PRINT_DEVELOPER, "QGL_Init()\n");
	if ( !QGL_Init( gl_driver->string ) )
	{
		QGL_Shutdown();

		ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string );

#ifdef _WIN32
		if (strcmp (gl_driver->string, "opengl32"))
		{
			ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - retrying with gl_driver opengl32\n");
			ri.Cvar_Set ("gl_driver", "opengl32");
			goto retryQGL;
		}
#endif
        
		return -1;
	}

	// initialize OS-specific parts of OpenGL
	ri.Con_Printf (PRINT_DEVELOPER, "GLimp_Init()\n");
	if ( !GLimp_Init( hinstance, hWnd ) )
	{
		ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init(): GLimp_Init() failed\n");
		QGL_Shutdown();
		return -1;
	}

	// set our "safe" modes
	gl_state.prev_mode = 3;

	// create the window and set up the context
	ri.Con_Printf (PRINT_DEVELOPER, "R_SetMode()\n");
	err = R_SetMode ();
	if (err != VID_ERR_NONE)
	{
		QGL_Shutdown();
		if (err & VID_ERR_RETRY_QGL)
			goto retryQGL;

        ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not R_SetMode()\n" );
		return -1;
	}

	ri.Con_Printf (PRINT_DEVELOPER, "Vid_MenuInit()\n");
	ri.Vid_MenuInit();

	/*
	** get our various GL strings
	*/
	gl_config.vendor_string = (const char *) qglGetString (GL_VENDOR);
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = (const char *) qglGetString (GL_RENDERER);
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = (const char *) qglGetString (GL_VERSION);
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = (const char *) qglGetString (GL_EXTENSIONS);
	//ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );

	Q_strncpy( renderer_buffer, gl_config.renderer_string, sizeof(renderer_buffer)-1);
	Q_strlwr( renderer_buffer );

	Q_strncpy( vendor_buffer, gl_config.vendor_string, sizeof(vendor_buffer)-1);
	Q_strlwr( vendor_buffer );

	if ( strstr( renderer_buffer, "voodoo" ) )
	{
		if ( !strstr( renderer_buffer, "rush" ) )
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	}
	else if ( strstr( vendor_buffer, "sgi" ) )
		gl_config.renderer = GL_RENDERER_SGI;
	else if ( strstr( renderer_buffer, "permedia" ) )
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if ( strstr( renderer_buffer, "glint" ) )
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if ( strstr( renderer_buffer, "glzicd" ) )
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if ( strstr( renderer_buffer, "gdi" ) )
		gl_config.renderer = GL_RENDERER_MCD;
	else if ( strstr( renderer_buffer, "pcx2" ) )
		gl_config.renderer = GL_RENDERER_PCX2;
	else if ( strstr( renderer_buffer, "verite" ) )
		gl_config.renderer = GL_RENDERER_RENDITION;
	else if ( strstr (vendor_buffer, "ati tech"))
		gl_config.renderer = GL_RENDERER_ATI;
	else if ( strstr (vendor_buffer, "nvidia corp"))
		gl_config.renderer = GL_RENDERER_NV;
	else
		gl_config.renderer = GL_RENDERER_OTHER;

	/*if ( toupper( gl_monolightmap->string[1] ) != 'F' )
	{
		if ( gl_config.renderer == GL_RENDERER_PERMEDIA2 )
		{
			ri.Cvar_Set( "gl_monolightmap", "A" );
			ri.Con_Printf( PRINT_ALL, "...using gl_monolightmap 'a'\n" );
		}
		else if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
		{
			ri.Cvar_Set( "gl_monolightmap", "0" );
		}
		else
		{
			ri.Cvar_Set( "gl_monolightmap", "0" );
		}
	}*/

	// power vr can't have anything stay in the framebuffer, so
	// the screen needs to redraw the tiled background every frame
	if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
	{
		ri.Cvar_Set( "scr_drawall", "1" );
	}
	else
	{
		ri.Cvar_Set( "scr_drawall", "0" );
	}

	// MCD has buffering issues
	if ( gl_config.renderer == GL_RENDERER_MCD )
	{
		ri.Cvar_SetValue( "gl_finish", 1 );
	}

	/*
	** grab extensions
	*/
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) || 
		 strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) )
	{
		ri.Con_Printf( PRINT_ALL, "...enabling GL_EXT_compiled_vertex_array\n" );
		qglLockArraysEXT = ( void (__stdcall *)(int, int) ) qwglGetProcAddress( "glLockArraysEXT" );
		qglUnlockArraysEXT = ( void (__stdcall *)(void) ) qwglGetProcAddress( "glUnlockArraysEXT" );
	}
	else
	{
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

#ifdef _WIN32
	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) )
	{
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
		ri.Con_Printf( PRINT_ALL, "...enabling WGL_EXT_swap_control\n" );
	}
	else
	{
		ri.Con_Printf( PRINT_ALL, "...WGL_EXT_swap_control not found\n" );
	}
#endif

	if ( strstr( gl_config.extensions_string, "GL_EXT_point_parameters" ) )
	{
		if (gl_config.renderer == GL_RENDERER_ATI)
			gl_ext_pointparameters = ri.Cvar_Get( "gl_ext_pointparameters", "0", CVAR_ARCHIVE );
		else
			gl_ext_pointparameters = ri.Cvar_Get( "gl_ext_pointparameters", "1", CVAR_ARCHIVE );

		if ( FLOAT_NE_ZERO(gl_ext_pointparameters->value) && (gl_config.renderer != GL_RENDERER_ATI || gl_ext_pointparameters->value == 2) )
		{
			qglPointParameterfEXT = ( void (APIENTRY *)( GLenum, GLfloat ) ) qwglGetProcAddress( "glPointParameterfEXT" );
			qglPointParameterfvEXT = ( void (APIENTRY *)( GLenum, const GLfloat * ) ) qwglGetProcAddress( "glPointParameterfvEXT" );
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_point_parameters\n" );
		}
		else
		{
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_point_parameters\n" );
		}
	}
	else
	{
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_point_parameters not found\n" );
	}

	/*if ( !qglColorTableEXT &&
		strstr( gl_config.extensions_string, "GL_EXT_paletted_texture" ) && 
		strstr( gl_config.extensions_string, "GL_EXT_shared_texture_palette" ) )
	{
		if ( gl_ext_palettedtexture->value )
		{
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_shared_texture_palette\n" );
			qglColorTableEXT = ( void ( APIENTRY * ) ( int, int, int, int, int, const void * ) ) qwglGetProcAddress( "glColorTableEXT" );
		}
		else
		{
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_shared_texture_palette\n" );
		}
	}
	else
	{
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_shared_texture_palette not found\n" );
	}*/

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) )
	{
		if ( FLOAT_NE_ZERO(gl_ext_multitexture->value) )
		{
			ri.Con_Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
			qglMTexCoord2fSGIS = ( void (__stdcall *)(GLenum, GLfloat, GLfloat) ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglMTexCoord2fvSGIS = ( void (__stdcall *)(GLenum, GLfloat*) ) qwglGetProcAddress( "glMultiTexCoord2fvARB" );
			qglActiveTextureARB = ( void (__stdcall *)(GLenum) ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void (__stdcall *)(GLenum) ) qwglGetProcAddress( "glClientActiveTextureARB" );
			GL_TEXTURE0 = GL_TEXTURE0_ARB;
			GL_TEXTURE1 = GL_TEXTURE1_ARB;
		}
		else
		{
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Con_Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_multitexture" ) )
	{
		if ( qglActiveTextureARB )
		{
			ri.Con_Printf( PRINT_ALL, "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n" );
		}
		else if ( FLOAT_NE_ZERO(gl_ext_multitexture->value) )
		{
			ri.Con_Printf( PRINT_ALL, "...using GL_SGIS_multitexture\n" );
			qglMTexCoord2fSGIS = ( void (__stdcall *)(GLenum, GLfloat, GLfloat) ) qwglGetProcAddress( "glMTexCoord2fSGIS" );
			qglMTexCoord2fvSGIS = ( void (__stdcall *)(GLenum, GLfloat*) ) qwglGetProcAddress( "glMTexCoord2fvSGIS" );
			qglSelectTextureSGIS = ( void (__stdcall *)(GLenum) ) qwglGetProcAddress( "glSelectTextureSGIS" );
			GL_TEXTURE0 = GL_TEXTURE0_SGIS;
			GL_TEXTURE1 = GL_TEXTURE1_SGIS;
		}
		else
		{
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_SGIS_multitexture\n" );
		}
	}
	else
	{
		ri.Con_Printf( PRINT_ALL, "...GL_SGIS_multitexture not found\n" );
	}

	ri.Con_Printf( PRINT_ALL, "Initializing r1gl extensions:\n" );

	/*gl_config.r1gl_GL_SGIS_generate_mipmap = false;
	if ( strstr( gl_config.extensions_string, "GL_SGIS_generate_mipmap" ) ) {
		if ( gl_ext_generate_mipmap->value ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_SGIS_generate_mipmap\n" );
			gl_config.r1gl_GL_SGIS_generate_mipmap = true;
		} else {
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_SGIS_generate_mipmap\n" );		
		}
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_SGIS_generate_mipmap not found\n" );
	}*/

	gl_config.r1gl_GL_ARB_point_sprite = false;
	if ( strstr( gl_config.extensions_string, "GL_ARB_point_sprite" ) )
	{
		//if ( gl_ext_point_sprite->value ) {
			qglPointParameterfARB = (void (__stdcall *)(GLenum,GLfloat))qwglGetProcAddress("glPointParameterfARB");
			qglPointParameterfvARB = (void (__stdcall *)(GLenum,const GLfloat *))qwglGetProcAddress("glPointParameterfvARB");
			if (!qglPointParameterfARB)
			{
				ri.Con_Printf( PRINT_ALL, "!!! qglGetProcAddress for GL_ARB_point_sprite failed\n" );
			}
			else
			{
				ri.Con_Printf( PRINT_ALL, "...using GL_ARB_point_sprite\n" );
				gl_config.r1gl_GL_ARB_point_sprite = true;
			}
		//} else {
		//	ri.Con_Printf( PRINT_ALL, "...ignoring GL_ARB_point_sprite\n" );		
		//}
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_ARB_point_sprite not found\n" );
	}

	gl_config.r1gl_GL_EXT_texture_filter_anisotropic = false;
	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( gl_ext_texture_filter_anisotropic->value ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n" );
			gl_config.r1gl_GL_EXT_texture_filter_anisotropic = true;
		} else {
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );		
		}
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
	gl_ext_texture_filter_anisotropic->modified = false;

	gl_config.r1gl_GL_ARB_texture_non_power_of_two = false;
	if ( strstr( gl_config.extensions_string, "GL_ARB_texture_non_power_of_two" ) ) {
		if (FLOAT_NE_ZERO (gl_ext_texture_non_power_of_two->value) ) {
			ri.Con_Printf( PRINT_ALL, "...using GL_ARB_texture_non_power_of_two\n" );
			gl_config.r1gl_GL_ARB_texture_non_power_of_two = true;
		} else {
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_ARB_texture_non_power_of_two\n" );		
		}
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_ARB_texture_non_power_of_two not found\n" );
	}

	if ( strstr (gl_config.extensions_string, "GL_ARB_occlusion_query"))
	{
		//r1: occlusion queries
		if (FLOAT_NE_ZERO (gl_ext_occlusion_query->value) )
		{
			qglGenQueriesARB			 = (void (__stdcall *)(GLsizei,GLuint *))qwglGetProcAddress ("glGenQueriesARB");
			qglGetQueryivARB			 = (void (__stdcall *)(GLenum,GLenum,GLint *))qwglGetProcAddress ("glGetQueryivARB");
			qglGetQueryObjectivARB		 = (void (__stdcall *)(GLuint,GLenum,GLint *))qwglGetProcAddress ("glGetQueryObjectivARB");
			qglBeginQueryARB			 = (void (__stdcall *)(GLenum,GLuint))qwglGetProcAddress ("glBeginQueryARB");
			qglEndQueryARB				 = (void (__stdcall *)(GLenum))qwglGetProcAddress ("glEndQueryARB");

			qglGetQueryivARB (GL_SAMPLES_PASSED, GL_QUERY_COUNTER_BITS, &gl_config.r1gl_QueryBits);
			ri.Con_Printf (PRINT_ALL, "...using GL_ARB_occlusion_query (%d bits)\n", gl_config.r1gl_QueryBits);
			if (gl_config.r1gl_QueryBits)
				qglGenQueriesARB (MAX_ENTITIES, gl_config.r1gl_Queries);
		}
		else
		{
			ri.Con_Printf (PRINT_ALL, "...ignoring GL_ARB_occlusion_query\n");
			gl_config.r1gl_QueryBits = 0;
		}
	}
	else
	{
		gl_config.r1gl_QueryBits = 0;
		ri.Con_Printf (PRINT_ALL, "...GL_ARB_occlusion_query not found\n");
	}

	ri.Con_Printf( PRINT_ALL, "Initializing r1gl NVIDIA-only extensions:\n" );
	gl_config.r1gl_GL_EXT_nv_multisample_filter_hint = false;
	if ( strstr( gl_config.extensions_string, "GL_NV_multisample_filter_hint" ) ) {
		gl_config.r1gl_GL_EXT_nv_multisample_filter_hint = true;	
		ri.Con_Printf( PRINT_ALL, "...allowing GL_NV_multisample_filter_hint\n" );
	} else {
		ri.Con_Printf( PRINT_ALL, "...GL_NV_multisample_filter_hint not found\n" );
	}

	ri.Con_Printf( PRINT_DEVELOPER, "GL_SetDefaultState()\n" );
	GL_SetDefaultState();

	//r1: setup cached screensizes
	vid_scaled_width = vid.width / gl_hudscale->value;
	vid_scaled_height = vid.height / gl_hudscale->value;

	/*
	** draw our stereo patterns
	*/
#if 0 // commented out until H3D pays us the money they owe us
	GL_DrawStereoPattern();
#endif

	ri.Con_Printf( PRINT_DEVELOPER, "GL_InitImages()\n" );
	GL_InitImages ();

	ri.Con_Printf( PRINT_DEVELOPER, "Mod_Init()\n" );
	Mod_Init ();

	ri.Con_Printf( PRINT_DEVELOPER, "R_InitParticleTexture()\n" );
	R_InitParticleTexture ();

	ri.Con_Printf( PRINT_DEVELOPER, "Draw_InitLocal()\n" );
	Draw_InitLocal ();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ri.Con_Printf (PRINT_ALL, "glGetError() = 0x%x\n", err);

	ri.Con_Printf( PRINT_DEVELOPER, "R_Init() complete.\n" );
	return 0;
}

/*
===============
R_Shutdown
===============
*/
void EXPORT R_Shutdown (void)
{
	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand ("screenshot");
	ri.Cmd_RemoveCommand ("imagelist");
	ri.Cmd_RemoveCommand ("gl_strings");
	ri.Cmd_RemoveCommand ("hash_stats");

#ifdef R1GL_RELEASE
	ri.Cmd_RemoveCommand ("r1gl_version");
#endif

	Mod_FreeAll ();

	GL_ShutdownImages ();

	/*
	** shut down OS specific OpenGL stuff like contexts, etc.
	*/
	GLimp_Shutdown();

	/*
	** shutdown our QGL subsystem
	*/
	QGL_Shutdown();
}

void GL_UpdateAnisotropy (void)
{
	int		i;
	image_t	*glt;
	float	value;

	if (!gl_config.r1gl_GL_EXT_texture_filter_anisotropic)
		value = 1;
	else
		value = gl_ext_max_anisotropy->value;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky)
		{
			GL_Bind (glt->texnum);
			qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, value);
		}
	}
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void EXPORT R_BeginFrame( float camera_separation )
{
#ifdef STEREO_SUPPORT
	gl_state.camera_separation = camera_separation;
#endif

	/*
	** change modes if necessary
	*/
	if ( gl_mode->modified || vid_fullscreen->modified )
	{	// FIXME: only restart if CDS is required
		cvar_t	*ref;

		ref = ri.Cvar_Get ("vid_ref", "r1gl", 0);
		ref->modified = true;
	}

	/*if ( gl_log->modified )
	{
		GLimp_EnableLogging( gl_log->value );
		gl_log->modified = false;
	}

	if ( gl_log->value )
	{
		GLimp_LogNewFrame();
	}*/

	if (gl_ext_nv_multisample_filter_hint->modified)
	{
		gl_ext_nv_multisample_filter_hint->modified = false;

		if (gl_config.r1gl_GL_EXT_nv_multisample_filter_hint)
		{
			if (!strcmp (gl_ext_nv_multisample_filter_hint->string, "nicest"))
				qglHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
			else
				qglHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_FASTEST);
		}
	}

	if (gl_contrast->modified)
	{
		if (gl_contrast->value < 0.5f)
			ri.Cvar_SetValue ("gl_contrast", 0.5f);
		else if (gl_contrast->value > 1.5f)
			ri.Cvar_SetValue ("gl_contrast", 1.5f);

		gl_contrast->modified = false;
	}



	/*
	** update 3Dfx gamma -- it is expected that a user will do a vid_restart
	** after tweaking this value
	*/
#if 0
	if ( vid_gamma->modified )
	{
		vid_gamma->modified = false;

		if ( gl_config.renderer & ( GL_RENDERER_VOODOO ) )
		{
			char envbuffer[1024];
			float g;

			g = 2.00 * ( 0.8 - ( vid_gamma->value - 0.5 ) ) + 1.0F;
			Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g );
			putenv( envbuffer );
			Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g );
			putenv( envbuffer );
		}
	}
#endif

#ifdef STEREO_SUPPORT
	GLimp_BeginFrame( camera_separation );
#else
	GLimp_BeginFrame ();
#endif

	/*
	** go into 2D mode
	*/
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	//qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	qglOrtho(0, vid_scaled_width, vid_scaled_height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();
	//qglDisable (GL_DEPTH_TEST);
	//GLPROFqglDisable (GL_CULL_FACE);
	//GLPROFqglDisable (GL_BLEND);
	//GLPROFqglEnable (GL_ALPHA_TEST);
	qglColor4fv(colorWhite);

	//qglEnable(GL_MULTISAMPLE_ARB);

	/*
	** draw buffer stuff
	*/
	if ( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = false;

#ifdef STEREO_SUPPORT
		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled )
#endif
		{
			if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	/*
	** texturemode stuff
	*/
	if ( gl_texturemode->modified )
	{
		GL_TextureMode( gl_texturemode->string );
		gl_texturemode->modified = false;
	}

	if (gl_ext_max_anisotropy->modified && gl_config.r1gl_GL_EXT_texture_filter_anisotropic)
	{
		GL_UpdateAnisotropy ();
		gl_ext_max_anisotropy->modified = false;
	}

	if (gl_ext_texture_filter_anisotropic->modified)
	{
		gl_config.r1gl_GL_EXT_texture_filter_anisotropic = false;
		if ( strstr( gl_config.extensions_string, "GL_EXT_texture_filter_anisotropic" ) )
		{
			if ( gl_ext_texture_filter_anisotropic->value )
			{
				ri.Con_Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n" );
				gl_config.r1gl_GL_EXT_texture_filter_anisotropic = true;
				GL_UpdateAnisotropy ();
			}
			else
			{
				ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
				GL_UpdateAnisotropy ();
			}
		}
		else
		{
			ri.Con_Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
		}
		
		gl_ext_texture_filter_anisotropic->modified = false;
	}

	if (gl_hudscale->modified)
	{
		int width, height;

		gl_hudscale->modified = false;

		if (gl_hudscale->value < 1.0f)
		{
			ri.Cvar_Set ("gl_hudscale", "1.0");
		}
		else
		{
			//r1: hudscaling
			width = (int)ceilf((float)vid.width / gl_hudscale->value);
			height = (int)ceilf((float)vid.height / gl_hudscale->value);

			//round to powers of 8/2 to avoid blackbars
			width = (width+7)&~7;
			height = (height+1)&~1;

			gl_hudscale->modified = false;

			vid_scaled_width = vid.width / gl_hudscale->value;
			vid_scaled_height = vid.height / gl_hudscale->value;

			// let the sound and input subsystems know about the new window
			ri.Vid_NewWindow (width, height);
		}
	}

#if 0
	if ( gl_texturealphamode->modified )
	{
		GL_TextureAlphaMode( gl_texturealphamode->string );
		gl_texturealphamode->modified = false;
	}

	if ( gl_texturesolidmode->modified )
	{
		GL_TextureSolidMode( gl_texturesolidmode->string );
		gl_texturesolidmode->modified = false;
	}
#endif

	if (gl_texture_formats->modified)
	{
		load_png_wals = strstr (gl_texture_formats->string, "png") ? true : false;
		load_jpg_wals = strstr (gl_texture_formats->string, "jpg") ? true : false;
		load_tga_wals = strstr (gl_texture_formats->string, "tga") ? true : false;
		gl_texture_formats->modified = false;
	}

	if (gl_pic_formats->modified)
	{
		load_png_pics = strstr (gl_pic_formats->string, "png") ? true : false;
		load_jpg_pics = strstr (gl_pic_formats->string, "jpg") ? true : false;
		load_tga_pics = strstr (gl_pic_formats->string, "tga") ? true : false;
		gl_pic_formats->modified = false;
	}

	/*
	** swapinterval stuff
	*/
	GL_UpdateSwapInterval();

	//
	// clear screen if desired
	//
	R_Clear ();
}

/*
=============
R_SetPalette
=============
*/
unsigned r_rawpalette[256];

void EXPORT R_SetPalette ( const unsigned char *palette)
{
	/*int		i;

	byte *rp = ( byte * ) r_rawpalette;

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = d_8to24table[i] & 0xff;
			rp[i*4+1] = ( d_8to24table[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table[i] >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}
	GL_SetTexturePalette( r_rawpalette );*/

	qglClearColor (0,0,0,0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor (1,0, 0.5 , 0.5);
}

/*
** R_DrawBeam
*/
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < 6; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0f/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglDepthMask( GL_FALSE );

	r = (float)(( d_8to24table[e->skinnum & 0xFF] ) & 0xFF);
	g = (float)(( d_8to24table[e->skinnum & 0xFF] >> 8 ) & 0xFF);
	b = (float)(( d_8to24table[e->skinnum & 0xFF] >> 16 ) & 0xFF);

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;

	qglColor4f( r, g, b, e->alpha );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE );
}

//===================================================================


void	EXPORT R_BeginRegistration (char *map);
struct model_s	* EXPORT R_RegisterModel (char *name);
struct image_s	* EXPORT R_RegisterSkin (char *name);
void EXPORT R_SetSky (char *name, float rotate, vec3_t axis);
void EXPORT	R_EndRegistration (void);

void EXPORT	R_RenderFrame (refdef_t *fd);

struct image_s	* EXPORT Draw_FindPic (char *name);

void	EXPORT Draw_Pic (int x, int y, char *name);
void	EXPORT Draw_Char (int x, int y, int c);
void	EXPORT Draw_TileClear (int x, int y, int w, int h, char *name);
void	EXPORT Draw_Fill (int x, int y, int w, int h, int c);
void	EXPORT Draw_FadeScreen (void);

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
void EXPORT GetExtraAPI (refimportnew_t rimp )
{
	if (rimp.APIVersion != EXTENDED_API_VERSION)
	{
		ri.Con_Printf (PRINT_ALL, "R1GL: ExtraAPI version number mismatch, expected version %d, got version %d.\n", EXTENDED_API_VERSION, rimp.APIVersion);
		return;
	}

	memcpy (&rx, &rimp, sizeof(rx));
}

refexport_t EXPORT GetRefAPI (refimport_t rimp )
{
	refexport_t	re;

	ri = rimp;

	re.api_version = API_VERSION;

	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;

	re.RenderFrame = R_RenderFrame;

	re.DrawGetPicSize = Draw_GetPicSize;
	re.DrawPic = Draw_Pic;
	re.DrawStretchPic = Draw_StretchPic;
	re.DrawChar = Draw_Char;
	re.DrawTileClear = Draw_TileClear;
	re.DrawFill = Draw_Fill;
	re.DrawFadeScreen= Draw_FadeScreen;

	re.DrawStretchRaw = Draw_StretchRaw;

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;

	re.CinematicSetPalette = R_SetPalette;
	re.BeginFrame = R_BeginFrame;
	re.EndFrame = GLimp_EndFrame;

	re.AppActivate = GLimp_AppActivate;

	Swap_Init ();

	return re;
}


#ifndef REF_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	ri.Sys_Error (ERR_FATAL, "%s", text);
}

void Com_Printf (const char *fmt, int level, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, level);
	vsprintf (text, fmt, argptr);
	va_end (argptr);

	ri.Con_Printf (PRINT_ALL, "%s", text);
}

#endif
