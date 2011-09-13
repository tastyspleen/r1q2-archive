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
// models.c -- model loading and caching

#include "gl_local.h"

int global_hax_texture_x = 0;
int global_hax_texture_y = 0;

model_t	*loadmodel;
int		modfilelen;

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, qboolean crash);

byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
model_t	mod_known[MAX_MOD_KNOWN];
int		mod_numknown;

// the inline * models from the current map are kept seperate
model_t	mod_inline[MAX_MOD_KNOWN];

int			registration_sequence;
qboolean	r_registering;
/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	cplane_t	*plane;
	
	if (!model || !model->nodes)
		ri.Sys_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	for (;;)
	{
		if (node->contents != -1)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (FLOAT_GT_ZERO(d))
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	//return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, model_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS],
		model);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	model_t	*mod;
	int		total, num;
	int		numbrush, numalias, numsprites, numsub;

	total = num = 0;
	numbrush = numalias = numsprites = numsub = 0;

	ri.Con_Printf (PRINT_ALL,"Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		num++;
		switch (mod->type)
		{
			case mod_brush:
				ri.Con_Printf (PRINT_ALL, "B ");
				numsub += mod->numsubmodels;
				numbrush++;
				break;
			case mod_sprite:
				ri.Con_Printf (PRINT_ALL, "S ");
				numsprites++;
				break;
			case mod_alias:
				ri.Con_Printf (PRINT_ALL, "A ");
				numalias++;
				break;
			default:
				ri.Con_Printf (PRINT_ALL, "! ");
				break;
		}
		ri.Con_Printf (PRINT_ALL, "%8i : %s\n", mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}

	ri.Con_Printf (PRINT_ALL, "%d brush models (B) with %d submodels, %d alias models (A), %d sprites (S)\n", numbrush, numsub, numalias, numsprites);
	ri.Con_Printf (PRINT_ALL, "Total resident: %i bytes (%.2f MB) in %d models (%d with submodels)\n", total, (float)total / 1024 / 1024, num, num + numsub);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

#define MODEL_HASH_SIZE	32

typedef struct mscache_s
{
	char				name[MAX_QPATH];
	struct mscache_s	*hash_next;
	int					size;
} mscache_t;

static model_t		*models_hash[MODEL_HASH_SIZE];
static mscache_t	*model_size_cache[MODEL_HASH_SIZE];

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t		*mod;
	model_t		*modelhash;
	mscache_t	*model_size;
	byte		*buf;
	int			i;
	unsigned	hash;
	
	if (!name || !name[0])
		ri.Sys_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numsubmodels)
			ri.Sys_Error (ERR_DROP, "bad inline model number %d", i);
		return &mod_inline[i];
	}

	fast_strlwr (name);

	hash = hashify (name) % MODEL_HASH_SIZE;

	for (modelhash = models_hash[hash]; modelhash; modelhash = modelhash->hash_next)
	{
		if (!strcmp (modelhash->name, name))
		{
			return modelhash;
		}
	}

	for (model_size = model_size_cache[hash]; model_size; model_size = model_size->hash_next)
	{
		if (!strcmp (model_size->name, name))
			break;
	}

	//
	// search the currently loaded models
	//
	/*for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0] || mod->hash != hash)
			continue;
		if (!strcmp (mod->name, name) )
			return mod;
	}*/
	
	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			break;	// free spot
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			ri.Sys_Error (ERR_DROP, "mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}

	strncpy (mod->name, name, sizeof(mod->name)-1);

	//
	// load the file
	//
	modfilelen = ri.FS_LoadFile (name, (void *)&buf);
	if (!buf)
	{
		if (crash)
			ri.Sys_Error (ERR_DROP, "Mod_NumForName: %s not found", mod->name);
		mod->name[0] = 0;
		return NULL;
	}
	
	loadmodel = mod;

	//
	// fill it in
	//


	// call the apropriate loader
	
	switch (LittleLong(*(unsigned *)buf))
	{
		case IDALIASHEADER:
			if (model_size)
				loadmodel->extradata = Hunk_Begin (model_size->size, model_size->size);
			else
				loadmodel->extradata = Hunk_Begin (0x200000, 0);
			Mod_LoadAliasModel (mod, buf);
			break;
			
		case IDSPRITEHEADER:
			if (model_size)
				loadmodel->extradata = Hunk_Begin (model_size->size, model_size->size);
			else
				loadmodel->extradata = Hunk_Begin (0x4000, 0);
			Mod_LoadSpriteModel (mod, buf);
			break;
		
		case IDBSPHEADER:
			if (model_size)
				loadmodel->extradata = Hunk_Begin (model_size->size, model_size->size);
			else
				loadmodel->extradata = Hunk_Begin (0x1000000, 0);
			Mod_LoadBrushModel (mod, buf);
			break;

		default:
			ri.Sys_Error (ERR_DROP,"Mod_NumForName: unknown 0x%.8x fileid for %s", LittleLong(*(unsigned *)buf), mod->name);
			break;
	}

	if (model_size)
	{
		loadmodel->extradatasize = model_size->size;
	}
	else
	{
		loadmodel->extradatasize = Hunk_End ();

		model_size = malloc (sizeof(*model_size));
		if (!model_size)
			ri.Sys_Error (ERR_FATAL, "Mod_ForName: out of memory");
		strcpy (model_size->name, mod->name);
		model_size->size = loadmodel->extradatasize;
		model_size->hash_next = model_size_cache[hash];

		model_size_cache[hash] = model_size;
	}

	mod->hash_next = models_hash[hash];
	models_hash[hash] = mod;

	ri.FS_FreeFile (buf);

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;


/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
#if Q_BIGENDIAN
	int		i;
#endif

	if (!l->filelen)
	{
		loadmodel->vis = NULL;
		return;
	}

	loadmodel->vis = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->vis, mod_base + l->fileofs, l->filelen);

#if Q_BIGENDIAN
	loadmodel->vis->numclusters = LittleLong (loadmodel->vis->numclusters);

	for (i=0 ; i<loadmodel->vis->numclusters ; i++)
	{
		loadmodel->vis->bitofs[i][0] = LittleLong (loadmodel->vis->bitofs[i][0]);
		loadmodel->vis->bitofs[i][1] = LittleLong (loadmodel->vis->bitofs[i][1]);
	}
#endif
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			count;

#if Q_BIGENDIAN
	int			i;
#endif

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadVertexes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

#if Q_BIGENDIAN
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
#else
	memcpy (out, in, sizeof(dvertex_t)*count);
#endif
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	vec3_t	corner;

	corner[0] = (float)(fabs(mins[0]) > fabs(maxs[0]) ? fabs(mins[0]) : fabs(maxs[0]));
	corner[1] = (float)(fabs(mins[1]) > fabs(maxs[1]) ? fabs(mins[1]) : fabs(maxs[1]));
	corner[2] = (float)(fabs(mins[2]) > fabs(maxs[2]) ? fabs(mins[2]) : fabs(maxs[2]));

	return VectorLength (corner);
}


/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	mmodel_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->mins[0] = LittleFloat (in->mins[0]) - 1;
		out->maxs[0] = LittleFloat (in->maxs[0]) + 1;
		out->origin[0] = LittleFloat (in->origin[0]);
		out->mins[1] = LittleFloat (in->mins[1]) - 1;
		out->maxs[1] = LittleFloat (in->maxs[1]) + 1;
		out->origin[1] = LittleFloat (in->origin[1]);
		out->mins[2] = LittleFloat (in->mins[2]) - 1;
		out->maxs[2] = LittleFloat (in->maxs[2]) + 1;
		out->origin[2] = LittleFloat (in->origin[2]);

		out->radius = RadiusFromBounds (out->mins, out->maxs);
		out->headnode = LittleLong (in->headnode);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
		out->visleafs = 0;
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadEdges: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);

	//r1: was count+1
	out = Hunk_Alloc (count * sizeof(*out));	

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

//FIXME: this is bad, loads entire file for just 8 bytes!
qboolean GetWalInfo (const char *name, int *width, int *height)
{
	if (rx.FS_FOpenFile)
	{
#ifdef _DEBUG
		int		i;
		char	grey = 8;
#endif
		miptex_t	mt;
		qboolean	closeFile;
		FILE		*h;

		rx.FS_FOpenFile (name, &h, HANDLE_OPEN, &closeFile);
		if (!h)
			return false;

		/*if (fread (&mt, sizeof(mt), 1, h) != 1)
		{
			ri.FS_FCloseFile (h);
			return false;
		}*/
		rx.FS_Read (&mt, sizeof(mt), h);

		if (closeFile)
			rx.FS_FCloseFile (h);
		
		*width = LittleLong (mt.width);
		*height = LittleLong (mt.height);

#ifdef _DEBUG
		FS_CreatePath (va("wals/%s", name));
		h = fopen (va("wals/%s", name), "wb");
		fwrite (&mt, 1, sizeof(mt), h);
		for (i = 0; i < mt.width * mt.height; i++)
			fwrite (&grey, 1, 1, h);
		for (i = 0; i < (mt.width * mt.height) / 2; i++)
			fwrite (&grey, 1, 1, h);
		for (i = 0; i < (mt.width * mt.height) / 4; i++)
			fwrite (&grey, 1, 1, h);
		for (i = 0; i < (mt.width * mt.height) / 8; i++)
			fwrite (&grey, 1, 1, h);
		fclose (h);
#endif

		return true;
	}
	else
	{
		miptex_t	*mt;

		ri.FS_LoadFile (name, (void **)&mt);

		if (!mt)
			return false;

		*width = LittleLong (mt->width);
		*height = LittleLong (mt->height);

		ri.FS_FreeFile ((void *)mt);
		return true;
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
//	image_t	*last_image;
	mtexinfo_t *out, *step;
	int 	i, count;
	char	name[MAX_QPATH];
	int		next;
	size_t	length;

	in = (void *)(mod_base + l->fileofs);
	
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadTexinfo: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
#if Q_BIGENDIAN
		out->vecs[0][0] = LittleFloat (in->vecs[0][0]);
		out->vecs[0][1] = LittleFloat (in->vecs[0][1]);
		out->vecs[0][2] = LittleFloat (in->vecs[0][2]);
		out->vecs[0][3] = LittleFloat (in->vecs[0][3]);

		out->vecs[1][0] = LittleFloat (in->vecs[1][0]);
		out->vecs[1][1] = LittleFloat (in->vecs[1][1]);
		out->vecs[1][2] = LittleFloat (in->vecs[1][2]);
		out->vecs[1][3] = LittleFloat (in->vecs[1][3]);
#else
		memcpy (out->vecs, in->vecs, sizeof(out->vecs));
#endif

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);

		if (next > 0)
			out->next = loadmodel->texinfo + next;
		else
		    out->next = NULL;

		fast_strlwr (in->texture);

		out->image = GL_FindImageBase (in->texture, it_wall);

		if (out->image)
			continue;

		Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);
		
		if (!GetWalInfo (name, &global_hax_texture_x, &global_hax_texture_y))
		{
			ri.Con_Printf (PRINT_ALL, "Couldn't load %s\n", name);
			out->image = r_notexture;
			continue;
		}

		length = strlen(name);

		if (load_tga_wals)
		{
			//Com_sprintf (name, sizeof(name), "textures/%s.tga", in->texture);
			memcpy (name + length-3, "tga", 3);
			out->image = GL_FindImage (name, in->texture, it_wall);
		}
		else
		{
			out->image = NULL;
		}

		if (!out->image)
		{
			if (load_png_wals)
			{
				memcpy (name + length-3, "png", 3);
				//Com_sprintf (name, sizeof(name), "textures/%s.png", in->texture);
				out->image = GL_FindImage (name, in->texture, it_wall);
			}

			if (!out->image)
			{
				if (load_jpg_wals)
				{
					memcpy (name + length-3, "jpg", 3);
					//Com_sprintf (name, sizeof(name), "textures/%s.jpg", in->texture);
					out->image = GL_FindImage (name, in->texture, it_wall);
				}

				if (!out->image)
				{
					memcpy (name + length-3, "wal", 3);
					//Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);
					out->image = GL_FindImage (name, in->texture, it_wall);
					
					if (!out->image)
					{
						ri.Con_Printf (PRINT_ALL, "Couldn't load %s\n", name);
						out->image = r_notexture;
					}
				}
			}
		}

		//last_image = out->image;

		global_hax_texture_x = global_hax_texture_y = 0;
	}

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = &loadmodel->texinfo[i];
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = (int)floor(mins[i]/16);
		bmaxs[i] = (int)ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;

//		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
//			ri.Sys_Error (ERR_DROP, "Bad surface extents");
	}
}


void GL_BuildPolygonFromSurface(msurface_t *fa);
void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_EndBuildingLightmaps (void);
void GL_BeginBuildingLightmaps (void);

/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;
	int			ti;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadFaces: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);

	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	currentmodel = loadmodel;

	GL_BeginBuildingLightmaps ();

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

		out->flags = 0;
		out->polys = NULL;

		out->texturechain = NULL;
		out->lightmapchain = NULL;
		out->dlight_s = 0;
		out->dlight_t = 0;
		out->dlightframe = 0;
		out->dlightbits = 0;

		out->visframe = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		ti = LittleShort (in->texinfo);
		if (ti < 0 || ti >= loadmodel->numtexinfo)
			ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: bad texinfo number");

		out->texinfo = loadmodel->texinfo + ti;

		CalcSurfaceExtents (out);
				
	// lighting info

		//for (i=0 ; i<MAXLIGHTMAPS ; i++)
			//out->styles[i] = in->styles[i];

		memcpy (out->styles, in->styles, sizeof(byte) * MAXLIGHTMAPS);

		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = loadmodel->lightdata + i;
		
	// set the drawing flags
		
		if (out->texinfo->flags & SURF_WARP)
		{
			out->flags |= SURF_DRAWTURB;
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface (out);	// cut up polygon for warps
		}

		// create lightmaps and polygons
		if (!(out->texinfo->flags & SURF_WARP)) 
		{
			if (!(out->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_DRAWSKY|SURF_DRAWTURB)))
			{
				GL_CreateSurfaceLightmap (out);
			}
			else
			{
				out->light_s = out->light_t = 0;
			}
		
			GL_BuildPolygonFromSurface(out);
		}
	}

	GL_EndBuildingLightmaps ();
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents != -1)
		return;

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadNodes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minmaxs[0] = LittleShort (in->mins[0]);
		out->minmaxs[1] = LittleShort (in->mins[1]);
		out->minmaxs[2] = LittleShort (in->mins[2]);

		out->minmaxs[3] = LittleShort (in->maxs[0]);
		out->minmaxs[4] = LittleShort (in->maxs[1]);
		out->minmaxs[5] = LittleShort (in->maxs[2]);
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		out->contents = -1;	// differentiate from leafs

		out->parent = NULL;
		out->visframe = 0;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, count;
//	glpoly_t	*poly;

	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadLeafs: funny lump size in %s",loadmodel->name);
	
	count = l->filelen / sizeof(*in);

	out = Hunk_Alloc ( count*sizeof(*out));	

	//memset (out, 0, count*sizeof(*out));

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minmaxs[0] = LittleShort (in->mins[0]);
		out->minmaxs[1] = LittleShort (in->mins[1]);
		out->minmaxs[2] = LittleShort (in->mins[2]);

		out->minmaxs[3] = LittleShort (in->maxs[0]);
		out->minmaxs[4] = LittleShort (in->maxs[1]);
		out->minmaxs[5] = LittleShort (in->maxs[2]);

		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);

		out->parent = NULL;
		out->visframe = 0;
		
		// gl underwater warp
#if 0
		if (out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA|CONTENTS_THINWATER) )
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
			{
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
				for (poly = out->firstmarksurface[j]->polys ; poly ; poly=poly->next)
					poly->flags |= SURF_UNDERWATER;
			}
		}
#endif
	}	
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j < 0 ||  j >= loadmodel->numsurfaces)
			ri.Sys_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{	
	int		count;
	int		*in, *out;

#if Q_BIGENDIAN
	int		i;
#endif
	
	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		ri.Sys_Error (ERR_DROP, "MOD_LoadBmodel: bad surfedges count in %s: %i",
		loadmodel->name, count);

	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

#if Q_BIGENDIAN
	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
#else
	memcpy (out, in, sizeof(int)*count);
#endif
}


/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		ri.Sys_Error (ERR_DROP, "Mod_LoadPlanes: funny lump size in %s",loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*2*sizeof(*out));	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (FLOAT_LT_ZERO(out->normal[j]))
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = (byte)LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i;
	dheader_t	*header;
	mmodel_t 	*bm;
	
	loadmodel->type = mod_brush;
	if (loadmodel != mod_known)
		ri.Sys_Error (ERR_DROP, "Loaded a brush model after the world");

	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	if (i != BSPVERSION)
		ri.Sys_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (byte *)header;

#ifdef Q_BIGENDIAN
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);
#endif

	//r1: check header pointers point within allocated data
	for (i = 0; i < MAX_LUMPS; i++)
	{
		//for some reason there are unused lumps with invalid values
		if (i == LUMP_POP)
			continue;

		if (header->lumps[i].fileofs < 0 || header->lumps[i].filelen < 0 ||
			header->lumps[i].fileofs + header->lumps[i].filelen > modfilelen)
			ri.Sys_Error (ERR_DROP, "Mod_LoadBrushModel: offset %d of size %d is out of bounds (%s is possibly truncated)", header->lumps[i].fileofs, header->lumps[i].filelen, mod->name);
	}

// load into heap
	
	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header->lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);
	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		model_t	*starmod;

		bm = &mod->submodels[i];
		starmod = &mod_inline[i];

		*starmod = *loadmodel;
		
		starmod->firstmodelsurface = bm->firstface;
		starmod->nummodelsurfaces = bm->numfaces;
		starmod->firstnode = bm->headnode;
		if (starmod->firstnode >= loadmodel->numnodes)
			ri.Sys_Error (ERR_DROP, "Inline model %i has bad firstnode", i);

		FastVectorCopy (bm->maxs, starmod->maxs);
		FastVectorCopy (bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0)
			*loadmodel = *starmod;

		starmod->numleafs = bm->visleafs;
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	unsigned int		i;
#if Q_BIGENDIAN
	int j;
#endif
	dmdl_t				header;
	dmdl_t				*pinmodel, *pheader;
	dstvert_t			*pinst, *poutst;
	dtriangle_t			*pintri, *pouttri;
	daliasframe_t		*pinframe, *poutframe;
	int					*pincmd, *poutcmd;
	int					version;
	unsigned int		required;
	char				*skin_name;

	pinmodel = (dmdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

	//pheader = Hunk_Alloc (size);
	pheader = &header;
	
	// byte swap the header fields and sanity check
#if Q_BIGENDIAN
	for (i=0 ; i<sizeof(dmdl_t)/4 ; i++)
		((int *)pheader)[i] = LittleLong (((int *)buffer)[i]);
#else
	memcpy (pheader, buffer, sizeof(dmdl_t));
#endif

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		ri.Con_Printf (PRINT_DEVELOPER, "model %s has a skin taller than traditional maximum of %d", mod->name,
				   MAX_LBM_HEIGHT);

	if (pheader->num_xyz <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no vertices", mod->name);

	if (pheader->num_xyz > MAX_VERTS)
		ri.Sys_Error (ERR_DROP, "model %s has too many vertices", mod->name);

	if (pheader->num_st <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no st vertices", mod->name);

	if (pheader->num_tris <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no triangles", mod->name);

	if (pheader->num_frames <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has no frames", mod->name);

	if (pheader->num_skins >= 31)
		ri.Sys_Error (ERR_DROP, "model %s has too many skins", mod->name);

	if (pheader->ofs_st <= 0 || pheader->ofs_frames <= 0 || pheader->ofs_glcmds <= 0 ||
		pheader->ofs_skins <= 0 || pheader->ofs_tris <= 0)
		ri.Sys_Error (ERR_DROP, "model %s has invalid offsets", mod->name);

	required = 0;

	required += sizeof(dmdl_t);

	required += pheader->num_st * sizeof(dstvert_t);
	required += pheader->num_tris * sizeof(dtriangle_t);
	required += pheader->num_frames * (sizeof(daliasframe_t)-4);

	//variable sized
	required += pheader->num_xyz * pheader->num_frames * sizeof(dtrivertx_t);

	required += pheader->num_glcmds * sizeof(int);
	required += pheader->num_skins * MAX_SKINNAME;

	if (pheader->ofs_end != required)
		ri.Sys_Error (ERR_DROP, "model %s has bad size header (%d != %d)", mod->name, pheader->ofs_end, required);

	if (pheader->ofs_frames + pheader->num_frames * sizeof(daliasframe_t) > required)
		ri.Sys_Error (ERR_DROP, "model %s has illegal frames offset", mod->name);

	if (pheader->ofs_glcmds + pheader->num_glcmds * sizeof(int) > required)
		ri.Sys_Error (ERR_DROP, "model %s has illegal glcmds offset", mod->name);
	
	if (pheader->ofs_skins + pheader->num_skins * MAX_SKINNAME > required)
		ri.Sys_Error (ERR_DROP, "model %s has illegal skins offset", mod->name);

	if (pheader->ofs_st + pheader->num_st * sizeof(dstvert_t) > required)
		ri.Sys_Error (ERR_DROP, "model %s has illegal vertices offset", mod->name);

	if (pheader->ofs_tris + pheader->num_tris * sizeof(dtriangle_t) > required)
		ri.Sys_Error (ERR_DROP, "model %s has illegal triangles offset", mod->name);

	if (pheader->framesize * pheader->num_frames != pheader->num_frames * (int)((sizeof(daliasframe_t)-4) + pheader->num_xyz*sizeof(dtrivertx_t)))
		ri.Sys_Error (ERR_DROP, "model %s has invalid frame size", mod->name);

	pheader = Hunk_Alloc (required);
	memcpy (pheader, &header, sizeof(dmdl_t));
	//
// load base s and t vertices (not used in gl version)
//
	pinst = (dstvert_t *) ((byte *)pinmodel + pheader->ofs_st);
	poutst = (dstvert_t *) ((byte *)pheader + pheader->ofs_st);

#if Q_BIGENDIAN
	for (i=0 ; i<pheader->num_st ; i++)
	{
		poutst[i].s = LittleShort (pinst[i].s);
		poutst[i].t = LittleShort (pinst[i].t);
	}
#else
	memcpy (poutst, pinst, pheader->num_st * sizeof(dstvert_t));
#endif

//
// load triangle lists
//
	pintri = (dtriangle_t *) ((byte *)pinmodel + pheader->ofs_tris);
	pouttri = (dtriangle_t *) ((byte *)pheader + pheader->ofs_tris);

#if Q_BIGENDIAN
	for (i=0 ; i<pheader->num_tris ; i++)
	{
		pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[0]);
		pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[0]);
		pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[1]);
		pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[1]);
		pouttri[i].index_xyz[j] = LittleShort (pintri[i].index_xyz[2]);
		pouttri[i].index_st[j] = LittleShort (pintri[i].index_st[2]);
	}
#else
	memcpy (pouttri, pintri, pheader->num_tris * sizeof(dtriangle_t));
#endif

//
// load the frames
//

	for (i=0 ; i<pheader->num_frames ; i++)
	{
		pinframe = (daliasframe_t *) ((byte *)pinmodel 
			+ pheader->ofs_frames + i * pheader->framesize);
		poutframe = (daliasframe_t *) ((byte *)pheader 
			+ pheader->ofs_frames + i * pheader->framesize);

#if Q_BIGENDIAN

		memcpy (poutframe->name, pinframe->name, sizeof(poutframe->name));
		for (j=0 ; j<3 ; j++)
		{
			poutframe->scale[j] = LittleFloat (pinframe->scale[j]);
			poutframe->translate[j] = LittleFloat (pinframe->translate[j]);
		}
		// verts are all 8 bit, so no swapping needed
		memcpy (poutframe->verts, pinframe->verts, 
			pheader->num_xyz*sizeof(dtrivertx_t));
#else
		memcpy (poutframe, pinframe, sizeof(daliasframe_t)-4);
		memcpy (poutframe->verts, pinframe->verts, pheader->num_xyz*sizeof(dtrivertx_t));
#endif
	}

	mod->type = mod_alias;

	//
	// load the glcmds
	//
	pincmd = (int *) ((byte *)pinmodel + pheader->ofs_glcmds);
	poutcmd = (int *) ((byte *)pheader + pheader->ofs_glcmds);

#if Q_BIGENDIAN
	for (i=0 ; i<pheader->num_glcmds ; i++)
		poutcmd[i] = LittleLong (pincmd[i]);
#else
	memcpy (poutcmd, pincmd, pheader->num_glcmds * sizeof(int));
#endif


	// register all skins
	memcpy ((char *)pheader + pheader->ofs_skins, (char *)pinmodel + pheader->ofs_skins,
		pheader->num_skins*MAX_SKINNAME);

	for (i=0 ; i<pheader->num_skins ; i++)
	{
		skin_name = (char *)pheader + pheader->ofs_skins + i*MAX_SKINNAME;
		fast_strlwr (skin_name);
		mod->skins[i] = GL_FindImage (skin_name, skin_name, it_skin);
	}

	mod->mins[0] = -32;
	mod->mins[1] = -32;
	mod->mins[2] = -32;

	mod->maxs[0] = 32;
	mod->maxs[1] = 32;
	mod->maxs[2] = 32;
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	dsprite_t	*sprin, *sprout;
	int			i;

	sprin = (dsprite_t *)buffer;
	sprout = Hunk_Alloc (modfilelen);

	sprout->ident = LittleLong (sprin->ident);
	sprout->version = LittleLong (sprin->version);
	sprout->numframes = LittleLong (sprin->numframes);

	if (sprout->version != SPRITE_VERSION)
		ri.Sys_Error (ERR_DROP, "sprite %s has wrong version number (%i should be %i)",
				 mod->name, sprout->version, SPRITE_VERSION);

	if (sprout->numframes > MAX_MD2SKINS)
		ri.Sys_Error (ERR_DROP, "sprite %s has too many frames (%i > %i)",
				 mod->name, sprout->numframes, MAX_MD2SKINS);

	if (sprout->numframes <= 0)
		ri.Sys_Error (ERR_DROP, "sprite %s has no frames", mod->name);

	// byte swap everything
	for (i=0 ; i<sprout->numframes ; i++)
	{
		sprout->frames[i].width = LittleLong (sprin->frames[i].width);
		sprout->frames[i].height = LittleLong (sprin->frames[i].height);
		sprout->frames[i].origin_x = LittleLong (sprin->frames[i].origin_x);
		sprout->frames[i].origin_y = LittleLong (sprin->frames[i].origin_y);
		memcpy (sprout->frames[i].name, sprin->frames[i].name, MAX_SKINNAME);
		fast_strlwr (sprout->frames[i].name);
		mod->skins[i] = GL_FindImage (sprout->frames[i].name, sprout->frames[i].name, it_sprite);

		//r1: sprites crash if they don't have valid skins for framenum so be noisy
		if (!mod->skins[i])
			ri.Con_Printf (PRINT_ALL, "GL_FindImage: Couldn't find skin '%s' for sprite '%s'\n", sprout->frames[i].name, mod->name);
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void EXPORT R_BeginRegistration (char *model)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;

	r_registering = true;

#ifdef RB_IMAGE_CACHE
	EmptyImageCache();
#endif

	registration_sequence++;
	r_oldviewcluster = -1;		// force markleafs

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", model);

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = ri.Cvar_Get ("flushmap", "0", 0);
	if ( strcmp(mod_known[0].name, fullname) || FLOAT_NE_ZERO(flushmap->value))
		Mod_Free (&mod_known[0]);
	r_worldmodel = Mod_ForName(fullname, true);

	r_viewcluster = -1;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s * EXPORT R_RegisterModel (char *name)
{
	model_t	*mod;
	int		i;
	dsprite_t	*sprout;
	dmdl_t		*pheader;

	mod = Mod_ForName (name, false);
	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		switch (mod->type)
		{
		case mod_brush:
			{
				for (i=0 ; i<mod->numtexinfo ; i++)
					mod->texinfo[i].image->registration_sequence = registration_sequence;
			}
			break;

		case mod_sprite:
			{
				sprout = (dsprite_t *)mod->extradata;
				for (i=0 ; i<sprout->numframes ; i++)
				{
					if (mod->skins[i])
						mod->skins[i]->registration_sequence  = registration_sequence;
				}
			}
			break;

		case mod_alias:
			{
				pheader = (dmdl_t *)mod->extradata;
				for (i=0 ; i<pheader->num_skins ; i++)
				{
					if (mod->skins[i])
						mod->skins[i]->registration_sequence  = registration_sequence;
				}
				mod->numframes = pheader->num_frames;
			}
			break;
		default:
            break;
		}
	}

	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void EXPORT R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i=0, mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;

		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Mod_Free (mod);
		}
	}

	GL_FreeUnusedImages ();
	r_registering = false;
}


//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	model_t		*hashstart;
	model_t		**prev;
	unsigned	hash;

	hash = hashify (mod->name) % MODEL_HASH_SIZE;
	
	prev = &models_hash[hash];
	for (;;)
	{
		hashstart = *prev;
		if (!hashstart)
			break;
		if (hashstart == mod)
		{
			*prev = hashstart->hash_next;
			break;
		}
		prev = &hashstart->hash_next;
	}

	Hunk_Free (mod->extradata);
	memset (mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll (void)
{
	int		i;

	for (i=0 ; i<mod_numknown ; i++)
	{
		if (mod_known[i].extradatasize)
			Mod_Free (&mod_known[i]);
	}
}
