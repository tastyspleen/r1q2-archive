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

//
// qfiles.h: quake file formats
// This file must be identical in the quake and utils directories
//

/*
========================================================================

The .pak files are just a linear collapse of a directory tree

========================================================================
*/

#ifdef _WIN32
#pragma pack (push,1)
#endif

#define IDPAKHEADER		(('K'<<24)+('C'<<16)+('A'<<8)+'P')

typedef struct dpackheader_s
{
	uint32		ident;		// == IDPAKHEADER
	uint32		dirofs;
	uint32		dirlen;
} PACKED_STRUCT dpackheader_t;

#define	MAX_FILES_IN_PACK	4096


/*
========================================================================

PCX files are used for as many images as possible

========================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    uint16	xmin,ymin,xmax,ymax;
    uint16	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    uint16	bytes_per_line;
    uint16	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} PACKED_STRUCT pcx_t;


/*
========================================================================

.MD2 triangle model file format

========================================================================
*/

#define IDALIASHEADER		(('2'<<24)+('P'<<16)+('D'<<8)+'I')
#define ALIAS_VERSION	8

#define	MAX_TRIANGLES	4096
#define MAX_VERTS		2048
#define MAX_FRAMES		512
#define MAX_MD2SKINS	32
#define	MAX_SKINNAME	64

typedef struct
{
	int16	s;
	int16	t;
} PACKED_STRUCT dstvert_t;

typedef struct 
{
	int16	index_xyz[3];
	int16	index_st[3];
} PACKED_STRUCT dtriangle_t;

typedef struct
{
	byte	v[3];			// scaled byte to fit in frame mins/maxs
	byte	lightnormalindex;
} PACKED_STRUCT dtrivertx_t;

#define DTRIVERTX_V0   0
#define DTRIVERTX_V1   1
#define DTRIVERTX_V2   2
#define DTRIVERTX_LNI  3
#define DTRIVERTX_SIZE 4

typedef struct
{
	float		scale[3];	// multiply byte verts by this
	float		translate[3];	// then add this
	char		name[16];	// frame name from grabbing
	dtrivertx_t	verts[1];	// variable sized
} PACKED_STRUCT daliasframe_t;


// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.


typedef struct
{
	int32			ident;
	int32			version;

	int32			skinwidth;
	int32			skinheight;
	int32			framesize;		// byte size of each frame

	int32			num_skins;
	int32			num_xyz;
	int32			num_st;			// greater than num_xyz for seams
	int32			num_tris;
	int32			num_glcmds;		// dwords in strip/fan command list
	int32			num_frames;

	uint32			ofs_skins;		// each skin is a MAX_SKINNAME string
	uint32			ofs_st;			// byte offset from start for stverts
	uint32			ofs_tris;		// offset for dtriangles
	uint32			ofs_frames;		// offset for first frame
	uint32			ofs_glcmds;	
	uint32			ofs_end;		// end of file
} PACKED_STRUCT dmdl_t;

/*
========================================================================

.SP2 sprite file format

========================================================================
*/

#define IDSPRITEHEADER	(('2'<<24)+('S'<<16)+('D'<<8)+'I')
		// little-endian "IDS2"
#define SPRITE_VERSION	2

typedef struct
{
	int32	width, height;
	int32	origin_x, origin_y;		// raster coordinates inside pic
	char	name[MAX_SKINNAME];		// name of pcx file
} PACKED_STRUCT dsprframe_t;

typedef struct {
	int32		ident;
	int32		version;
	int32		numframes;
	dsprframe_t	frames[1];			// variable sized
} PACKED_STRUCT dsprite_t;

/*
==============================================================================

  .WAL texture file format

==============================================================================
*/


#define	MIPLEVELS	4
typedef struct miptex_s
{
	char		name[32];
	uint32		width, height;
	uint32		offsets[MIPLEVELS];		// four mip maps stored
	char		animname[32];			// next frame in animation chain
	int32		flags;
	int32		contents;
	int32		value;
} PACKED_STRUCT miptex_t;



/*
==============================================================================

  .BSP file format

==============================================================================
*/

#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
		// little-endian "IBSP"

#define BSPVERSION	38


// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define	MAX_MAP_MODELS		1024
#define	MAX_MAP_BRUSHES		8192
#define	MAX_MAP_ENTITIES	2048
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_TEXINFO		8192

#define	MAX_MAP_AREAS		256
#define	MAX_MAP_AREAPORTALS	1024
#define	MAX_MAP_PLANES		65536
#define	MAX_MAP_NODES		65536
#define	MAX_MAP_BRUSHSIDES	65536
#define	MAX_MAP_LEAFS		65536
#define	MAX_MAP_VERTS		65536
#define	MAX_MAP_FACES		65536
#define	MAX_MAP_LEAFFACES	65536
#define	MAX_MAP_LEAFBRUSHES 65536
#define	MAX_MAP_PORTALS		65536
#define	MAX_MAP_EDGES		128000
#define	MAX_MAP_SURFEDGES	256000
#define	MAX_MAP_LIGHTING	0x200000
#define	MAX_MAP_VISIBILITY	0x100000

// key / value pair sizes

//#define	MAX_KEY		32
//#define	MAX_VALUE	1024

//=============================================================================

typedef struct
{
	int32	fileofs;
	int32	filelen;
} PACKED_STRUCT lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES			1
#define	LUMP_VERTEXES		2
#define	LUMP_VISIBILITY		3
#define	LUMP_NODES			4
#define	LUMP_TEXINFO		5
#define	LUMP_FACES			6
#define	LUMP_LIGHTING		7
#define	LUMP_LEAFS			8
#define	LUMP_LEAFFACES		9
#define	LUMP_LEAFBRUSHES	10
#define	LUMP_EDGES			11
#define	LUMP_SURFEDGES		12
#define	LUMP_MODELS			13
#define	LUMP_BRUSHES		14
#define	LUMP_BRUSHSIDES		15
#define	LUMP_POP			16
#define	LUMP_AREAS			17
#define	LUMP_AREAPORTALS	18
#define	HEADER_LUMPS		19

typedef struct
{
	int32		ident;
	int32		version;	
	lump_t		lumps[HEADER_LUMPS];
} PACKED_STRUCT dheader_t;

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];		// for sounds or lights
	int32		headnode;
	int32		firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
} PACKED_STRUCT dmodel_t;


typedef struct
{
	float	point[3];
} PACKED_STRUCT dvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

// planes (x&~1) and (x&~1)+1 are always opposites

typedef struct
{
	float	normal[3];
	float	dist;
	uint32	type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} PACKED_STRUCT dplane_t;


// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			2		// translucent, but not watery
#define	CONTENTS_AUX			4
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_MIST			64
#define	LAST_VISIBLE_CONTENTS	64

// remaining contents are non-visible, and don't eat brushes

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define	CONTENTS_CURRENT_0		0x40000
#define	CONTENTS_CURRENT_90		0x80000
#define	CONTENTS_CURRENT_180	0x100000
#define	CONTENTS_CURRENT_270	0x200000
#define	CONTENTS_CURRENT_UP		0x400000
#define	CONTENTS_CURRENT_DOWN	0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define	CONTENTS_DEADMONSTER	0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000



#define	SURF_LIGHT		0x1		// value will hold the light strength

#define	SURF_SLICK		0x2		// effects game physics

#define	SURF_SKY		0x4		// don't draw, but add to skybox
#define	SURF_WARP		0x8		// turbulent water warp
#define	SURF_TRANS33	0x10
#define	SURF_TRANS66	0x20
#define	SURF_FLOWING	0x40	// scroll towards angle
#define	SURF_NODRAW		0x80	// don't bother referencing the texture




typedef struct
{
	int32		planenum;
	int32		children[2];	// negative numbers are -(leafs+1), not nodes
	int16		mins[3];		// for frustom culling
	int16		maxs[3];
	uint16		firstface;
	uint16		numfaces;	// counting both sides
} PACKED_STRUCT dnode_t;


typedef struct texinfo_s
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int32		flags;			// miptex flags + overrides
	int32		value;			// light emission, etc
	char		texture[32];	// texture name (textures/whatever.wal)
	int32		nexttexinfo;	// for animations, -1 = end of chain
} PACKED_STRUCT texinfo_t;


// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	uint16		v[2];		// vertex numbers
} PACKED_STRUCT dedge_t;

#define	MAXLIGHTMAPS	4
typedef struct
{
	uint16		planenum;
	int16		side;

	int32			firstedge;		// we must support > 64k edges
	int16		numedges;	
	int16		texinfo;

// lighting info
	byte		styles[MAXLIGHTMAPS];
	int32		lightofs;		// start of [numstyles*surfsize] samples
} PACKED_STRUCT dface_t;

typedef struct
{
	int32		contents;			// OR of all brushes (not needed?)

	int16		cluster;
	int16		area;

	int16		mins[3];			// for frustum culling
	int16		maxs[3];

	uint16		firstleafface;
	uint16		numleaffaces;

	uint16		firstleafbrush;
	uint16		numleafbrushes;
} PACKED_STRUCT dleaf_t;

typedef struct
{
	uint16		planenum;		// facing out of the leaf
	int16		texinfo;
} PACKED_STRUCT dbrushside_t;

typedef struct
{
	int32		firstside;
	int32		numsides;
	int32		contents;
} dbrush_t;

#define	ANGLE_UP	-1
#define	ANGLE_DOWN	-2


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define	DVIS_PVS	0
#define	DVIS_PHS	1
typedef struct
{
	int32			numclusters;
	int32			bitofs[8][2];	// bitofs[numclusters][2]
} PACKED_STRUCT dvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
	int32		portalnum;
	int32		otherarea;
} PACKED_STRUCT dareaportal_t;

typedef struct
{
	int32		numareaportals;
	int32		firstareaportal;
} PACKED_STRUCT darea_t;

#ifdef _WIN32
#pragma pack (pop)
#endif


