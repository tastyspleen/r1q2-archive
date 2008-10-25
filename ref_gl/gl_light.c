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
// r_light.c

#include "gl_local.h"

int	r_dlightframecount;

#define	DLIGHT_CUTOFF	64

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

void R_RenderDlight (dlight_t *light)
{
	int		i, j;
	float	a;
	vec3_t	v;
	float	rad;

	rad = light->intensity * 0.35f;

	VectorSubtract (light->origin, r_origin, v);
#if 0
	// FIXME?
	if (VectorLength (v) < rad)
	{	// view is inside the dlight
		V_AddBlend (light->color[0], light->color[1], light->color[2], light->intensity * 0.0003, v_blend);
		return;
	}
#endif

	qglBegin (GL_TRIANGLE_FAN);
	qglColor3f (light->color[0]*0.2f, light->color[1]*0.2f, light->color[2]*0.2f);
	for (i=0 ; i<3 ; i++)
		v[i] = light->origin[i] - vpn[i]*rad;
	qglVertex3fv (v);
	qglColor3f (0,0,0);
	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0f * M_PI*2;
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + vright[j]*(float)cos(a)*rad
				+ vup[j]*(float)sin(a)*rad;
		qglVertex3fv (v);
	}
	qglEnd ();
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;

	if (FLOAT_EQ_ZERO(gl_flashblend->value))
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame
	qglDepthMask (0);
	qglDisable (GL_TEXTURE_2D);
	qglShadeModel (GL_SMOOTH);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ONE, GL_ONE);

	l = r_newrefdef.dlights;
	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_RenderDlight (l);

	qglColor3f (1,1,1);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (1);
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
	
	if (node->contents != -1)
		return;

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->intensity-DLIGHT_CUTOFF)
	{
		R_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->intensity+DLIGHT_CUTOFF)
	{
		R_MarkLights (light, bit, node->children[1]);
		return;
	}
		
// mark the polygons
	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		/*dist = DotProduct (light->origin, surf->plane->normal) - surf->plane->dist;	//Discoloda
		if (dist >= 0)									//Discoloda
			sidebit = 0;								//Discoloda
		else										//Discoloda
			sidebit = SURF_PLANEBACK;						//Discoloda

		if ( (surf->flags & SURF_PLANEBACK) != sidebit )				//Discoloda
			continue;								//Discoloda*/

		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights (light, bit, node->children[0]);
	R_MarkLights (light, bit, node->children[1]);
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	if (FLOAT_NE_ZERO(gl_flashblend->value))
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame
	l = r_newrefdef.dlights;
	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_MarkLights ( l, 1<<i, r_worldmodel->nodes );
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t			pointcolor;
cplane_t		*lightplane;		// used as shadow plane
vec3_t			lightspot;

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	int			side;
	cplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	int			maps;
	int			r;

	if (node->contents != -1)
		return -1;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = FLOAT_LT_ZERO(front);
	
	if ((FLOAT_LT_ZERO (back)) == side)
		return RecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = RecursiveLightPoint (node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ((FLOAT_LT_ZERO (back)) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node
	FastVectorCopy (mid, lightspot);
	lightplane = plane;

	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&(SURF_DRAWTURB|SURF_DRAWSKY)) 
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = (int)(DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3]);
		t = (int)(DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3]);

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorClear (pointcolor);

		if (lightmap)
		{
			vec3_t scale;

			lightmap += 3*(dt * ((surf->extents[0]>>4)+1) + ds);

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				scale[0] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[0];
				scale[1] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[1];
				scale[2] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[2];

				pointcolor[0] += lightmap[0] * scale[0] * 0.003921568627450980392156862745098f;
				pointcolor[1] += lightmap[1] * scale[1] * 0.003921568627450980392156862745098f;
				pointcolor[2] += lightmap[2] * scale[2] * 0.003921568627450980392156862745098f;
				lightmap += 3*((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1);
			}
		}
		
		return 1;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

/*
===============
R_LightPoint
===============
*/
void R_LightPoint (vec3_t p, vec3_t color)
{
	vec3_t		end;
	int			r;
	int			lnum;
	dlight_t	*dl;
	//float		light;
	vec3_t		dist;
	float		add;
	
	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0f;
		return;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	
	r = RecursiveLightPoint (r_worldmodel->nodes, p, end);
	
	if (r == -1)
	{
		VectorClear (color);
	}
	else
	{
		FastVectorCopy (pointcolor, *color);
	}

	//
	// add dynamic lights
	//
	//light = 0;
	if (FLOAT_NE_ZERO (gl_dynamic->value))
	{
		dl = r_newrefdef.dlights;
		for (lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++, dl++)
		{
			VectorSubtract (currententity->origin,
							dl->origin,
							dist);
			add = dl->intensity - VectorLength(dist);
			add *= (1.0f/256);
			if (FLOAT_GT_ZERO(add))
			{
				VectorMA (color, add, dl->color, color);
			}
		}
	}

	if (FLOAT_NE_ZERO(gl_doublelight_entities->value))
		VectorScale (color, gl_modulate->value, color);

	if (usingmodifiedlightmaps)
	{
		float		max, r, g, b;

		r = color[0];
		g = color[1];
		b = color[2];

		max = r + g + b;
		max /= 3;
		if (FLOAT_EQ_ZERO (gl_coloredlightmaps->value))
		{
			color[0] = color[1] = color[2] = max;
		}
		else
		{
			color[0] = max + (r - max) * gl_coloredlightmaps->value;
			color[1] = max + (g - max) * gl_coloredlightmaps->value;
			color[2] = max + (b - max) * gl_coloredlightmaps->value;
		}
	}
}


//===================================================================

#define BLOCKLIGHT_SIZE 3

#ifdef WIN32
__declspec(align(16)) static float s_blocklights[34*34*BLOCKLIGHT_SIZE];
#else
static float s_blocklights[34*34*BLOCKLIGHT_SIZE];
#endif

#define INTEGER_DLIGHTS		1

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;

#ifdef INTEGER_DLIGHTS
	int			fdist, frad, fminlight;
	int			fsacc, ftacc;
	int			local[3];
#else
	float		fdist, frad, fminlight;
	float		fsacc, ftacc;
	vec3_t		local;
#endif

	vec3_t		impact;

	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;
	dlight_t	*dl;
	//float		*pfBL;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		dl = &r_newrefdef.dlights[lnum];

#ifdef INTEGER_DLIGHTS
		if (FLOAT_NE_ZERO (gl_dlight_falloff->value))
			frad = Q_ftol(dl->intensity * 1.10f);
		else
			frad = Q_ftol(dl->intensity);

#else
		if (FLOAT_NE_ZERO (gl_dlight_falloff->value))
			frad = dl->intensity * 1.10f;
		else
			frad = dl->intensity;
#endif

		fdist = (int)(DotProduct (dl->origin, surf->plane->normal) -
				surf->plane->dist);

#ifdef INTEGER_DLIGHTS
		frad -= abs(fdist);
#else
		frad -= fabs(fdist);
#endif
		// rad is now the highest intensity on the plane

		fminlight = DLIGHT_CUTOFF;	// FIXME: make configurable?

		if (frad < fminlight)
			continue;

		fminlight = frad - fminlight;

		//for (i=0 ; i<3 ; i++)
		impact[0] = dl->origin[0] - surf->plane->normal[0]*fdist;
		impact[1] = dl->origin[1] - surf->plane->normal[1]*fdist;
		impact[2] = dl->origin[2] - surf->plane->normal[2]*fdist;

		local[0] = (int)(DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0]);
		local[1] = (int)(DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1]);

		//pfBL = s_blocklights;
		i = 0;
		for (t = 0, ftacc = 0 ; t<tmax ; ftacc += 16, t++)
		{
			td = abs(local[1] - ftacc);
			//if ( td < 0 )
			//	td = -td;
			//td = abs(td);

			//for ( s=0, fsacc = 0 ; s<smax ; fsacc += 16, pfBL += 3, s++)
			s = 0;
			fsacc = 0;
			for (;;)
			{
				if (s++ == smax)
					break;
#ifdef INTEGER_DLIGHTS
				sd = abs(local[0] - fsacc);
#else
				sd = Q_ftol (local[0] - fsacc);
#endif

				//if ( sd < 0 )
				//	sd = -sd;
				//sd = abs(sd);

				if (sd > td)
					fdist = sd + (td>>1);
				else
					fdist = td + (sd>>1);

				if ( fdist < fminlight)
				{
					if (FLOAT_EQ_ZERO (gl_dlight_falloff->value))
					{
						s_blocklights[i++] += ( frad - fdist ) * dl->color[0];
						s_blocklights[i++] += ( frad - fdist ) * dl->color[1];
						s_blocklights[i++] += ( frad - fdist ) * dl->color[2];
					}
					else
					{
						s_blocklights[i++] += ( fminlight - fdist ) * dl->color[0];
						s_blocklights[i++] += ( fminlight - fdist ) * dl->color[1];
						s_blocklights[i++] += ( fminlight - fdist ) * dl->color[2];
					}
#if BLOCKLIGHT_SIZE == 4
					i ++;
#endif
				}
				else
				{
					i += BLOCKLIGHT_SIZE;
				}

				fsacc += 16;
			}
		}
	}
}


/*
** R_SetCacheState
*/
void R_SetCacheState( msurface_t *surf )
{
	int maps;

	for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
		 maps++)
	{
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	//int			r, g, b, a, max;
	int			max;
	int			colors[4];
	int			i, j, size;
	byte		*lightmap;
	float		scale[4];
	int			nummaps;
	float		*bl;

	if ( surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) )
		ri.Sys_Error (ERR_DROP, "R_BuildLightMap called for non-lit surface");

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	size = smax*tmax;

	if (size > (sizeof(s_blocklights)>>4) )
		ri.Sys_Error (ERR_DROP, "R_BuildLightMap: Bad s_blocklights size %d", size);

// set to full bright if no light data
	if (!surf->samples)
	{
//		int maps;

		for (i=0 ; i<size*BLOCKLIGHT_SIZE ; i++)
			s_blocklights[i] = 255;
		/*for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			//style = &r_newrefdef.lightstyles[surf->styles[maps]];
		}*/
		goto store;
	}

	// count the # of maps
	for ( nummaps = 0 ; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255 ;
		 nummaps++)
		;

	lightmap = surf->samples;

	// add all the lightmaps
	if ( nummaps == 1 )
	{
		int maps;

		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			bl = s_blocklights;

			scale[0] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[0];
			scale[1] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[1];
			scale[2] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[2];

			if ( scale[0] == 1.0F &&
				 scale[1] == 1.0F &&
				 scale[2] == 1.0F )
			{
				for (i=0 ; i<size; i++, bl+=3)
				{
					bl[0] = lightmap[i*3+0];
					bl[1] = lightmap[i*3+1];
					bl[2] = lightmap[i*3+2];
				}
			}
			else
			{
				for (i=0 ; i<size; i++, bl+=3)
				{
					bl[0] = lightmap[i*3+0] * scale[0];
					bl[1] = lightmap[i*3+1] * scale[1];
					bl[2] = lightmap[i*3+2] * scale[2];
				}
			}
			lightmap += size*3;		// skip to next lightmap
		}
	}
	else
	{
		int maps;

		memset( s_blocklights, 0, sizeof( s_blocklights[0] ) * size * BLOCKLIGHT_SIZE );

		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			bl = s_blocklights;

			scale[0] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[0];
			scale[1] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[1];
			scale[2] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[2];

			if ( scale[0] == 1.0F &&
				 scale[1] == 1.0F &&
				 scale[2] == 1.0F )
			{
				for (i=0 ; i<size ; i++, bl+=3 )
				{
					bl[0] += lightmap[i*3+0];
					bl[1] += lightmap[i*3+1];
					bl[2] += lightmap[i*3+2];
				}
			}
			else
			{
				for (i=0 ; i<size ; i++, bl+=3)
				{
					bl[0] += lightmap[i*3+0] * scale[0];
					bl[1] += lightmap[i*3+1] * scale[1];
					bl[2] += lightmap[i*3+2] * scale[2];
				}
			}
			lightmap += size*3;		// skip to next lightmap
		}
	}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// put into texture format
store:
	stride -= (smax<<2);
	bl = s_blocklights;

	//monolightmap = gl_monolightmap->string[0];

	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++)
		{
			Q_fastfloats (bl, colors);

			// catch negative lights
			if (colors[0] < 0)
				colors[0] = 0;

			if (colors[1] < 0)
				colors[1] = 0;

			if (colors[2] < 0)
				colors[2] = 0;

			/*
			** determine the brightest of the three color components
			*/
			if (colors[0] > colors[1])
				max = colors[0];
			else
				max = colors[1];
			if (colors[2] > max)
				max = colors[2];

			/*
			** alpha is ONLY used for the mono lightmap case.  For this reason
			** we set it to the brightest of the color components so that 
			** things don't get too dim.
			*/
			//a = max;
			colors[3] = max;

			/*
			** rescale all the color components if the intensity of the greatest
			** channel exceeds 1.0
			*/
			if (max > 255)
			{
				float t = 255.0F / max;

				colors[0] = Q_ftol(colors[0]*t);
				colors[1] = Q_ftol(colors[1]*t);
				colors[2] = Q_ftol(colors[2]*t);
				colors[3] = Q_ftol(colors[3]*t);
			}

			if (!usingmodifiedlightmaps)
			{
				dest[0] = colors[0];
				dest[1] = colors[1];
				dest[2] = colors[2];
			}
			else
			{
				//max = colors[0] + colors[1] + colors[2];
				//max /= 3;
				if (FLOAT_NE_ZERO (gl_r1gl_test->value))
					max = (int)(0.289f * colors[0] + 0.587f * colors[1] + 0.114f * colors[2]);
				else
					max = (colors[0] + colors[1] + colors[2]) / 3;
				if (FLOAT_EQ_ZERO (gl_coloredlightmaps->value))
				{
					dest[0] = dest[1] = dest[2] = max;
				}
				else
				{
					dest[0] = (byte)Q_ftol(max + (colors[0] - max) * gl_coloredlightmaps->value);
					dest[1] = (byte)Q_ftol(max + (colors[1] - max) * gl_coloredlightmaps->value);
					dest[2] = (byte)Q_ftol(max + (colors[2] - max) * gl_coloredlightmaps->value);
				}
			}

			dest[3] = colors[3];

			bl += BLOCKLIGHT_SIZE;
			dest += 4;
		}
	}
}

