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

#ifdef _WIN32
#  include <windows.h>
#endif

#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

#ifndef __linux__
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif
#endif

#include "../client/ref.h"

#include "qgl.h"

//please keep this undefined on modified versions.
#define R1GL_RELEASE 1

#ifdef R1GL_RELEASE
#define	REF_VERSION	"R1GL 0.1.5.42"
#else
#define REF_VERSION "R1GL015-modified"
#endif

#define	MAX_TEXTURE_DIMENSIONS	1024

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	unsigned		width, height;			// coordinates from main game
} viddef_t;
#endif

typedef float vec4_t[4];

extern	viddef_t	vid;

/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

typedef enum 
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef struct image_s
{
	char	name[MAX_QPATH];			// game path, including extension
	char	basename[MAX_QPATH];				// as referenced by texinfo
	imagetype_t	type;
	int		width, height;				// source image
	int		upload_width, upload_height;	// after power of two and picmip
	int		registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	unsigned long		texnum;						// gl texture binding
	//int		detailtexnum;
	float	sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
	//qboolean	scrap;
	int		has_alpha;
	//unsigned int hash;
	struct image_s	*hash_next;
} image_t;

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS		1152
#define	TEXNUM_IMAGES		1153
//#define TEXNUM_DETAIL		5555
#define		MAX_GLTEXTURES	1024

//extern	cvar_t	*con_alpha;

extern	vec4_t	colorWhite;

extern	qboolean load_png_pics;
extern	qboolean load_tga_pics;
extern	qboolean load_jpg_pics;

extern	qboolean load_png_wals;
extern	qboolean load_tga_wals;
extern	qboolean load_jpg_wals;


//===================================================================

#define	VID_ERR_NONE				0
#define	VID_ERR_FAIL				1
#define	VID_ERR_RETRY_QGL			2
#define VID_ERR_FULLSCREEN_FAILED	4
#define VID_ERR_INVALID_MODE		8

#include "gl_model.h"

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

void GL_SetDefaultState( void );
void GL_UpdateSwapInterval( void );

extern	double	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;


#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


//====================================================

extern	image_t		gltextures[MAX_GLTEXTURES];
extern	int			numgltextures;


extern	image_t		*r_notexture;
extern	image_t		*r_particletexture;
extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;


extern	int			gl_filter_min, gl_filter_max;

extern	qboolean	r_registering;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;

extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_vertex_arrays;

//extern cvar_t	*gl_ext_swapinterval;
//extern cvar_t	*gl_ext_palettedtexture;
extern cvar_t	*gl_ext_multitexture;
extern cvar_t	*gl_ext_pointparameters;
//extern cvar_t	*gl_ext_compiled_vertex_array;

//r1ch: my extensions
//extern cvar_t	*gl_ext_generate_mipmap;
extern cvar_t	*gl_ext_point_sprite;
extern cvar_t	*gl_ext_texture_filter_anisotropic;
extern cvar_t	*gl_ext_texture_non_power_of_two;
extern cvar_t	*gl_ext_max_anisotropy;
extern cvar_t	*gl_ext_nv_multisample_filter_hint;
extern cvar_t	*gl_ext_occlusion_query;

extern cvar_t	*gl_colorbits;
extern cvar_t	*gl_alphabits;
extern cvar_t	*gl_depthbits;
extern cvar_t	*gl_stencilbits;

extern cvar_t	*gl_ext_multisample;
extern cvar_t	*gl_ext_samples;

extern cvar_t	*gl_r1gl_test;
extern cvar_t	*gl_doublelight_entities;
extern cvar_t	*gl_noscrap;
extern cvar_t	*gl_zfar;
extern cvar_t	*gl_overbrights;
extern cvar_t	*gl_linear_mipmaps;
extern cvar_t	*gl_hudscale;

extern cvar_t	*vid_gamma_pics;

extern cvar_t	*gl_forcewidth;
extern cvar_t	*gl_forceheight;

extern cvar_t	*vid_topmost;

extern cvar_t	*gl_particle_min_size;
extern cvar_t	*gl_particle_max_size;
extern cvar_t	*gl_particle_size;
extern cvar_t	*gl_particle_att_a;
extern cvar_t	*gl_particle_att_b;
extern cvar_t	*gl_particle_att_c;

//extern	cvar_t	*gl_nosubimage;
extern	cvar_t	*gl_bitdepth;
extern	cvar_t	*gl_mode;
//extern	cvar_t	*gl_log;
//extern	cvar_t	*gl_lightmap;
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_dynamic;
//extern  cvar_t  *gl_monolightmap;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_picmip;
extern	cvar_t	*gl_skymip;
extern	cvar_t	*gl_showtris;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_ztrick;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
//extern	cvar_t	*gl_poly;
//extern	cvar_t	*gl_texsort;
extern	cvar_t	*gl_polyblend;
extern	cvar_t	*gl_flashblend;
//extern	cvar_t	*gl_lightmaptype;
extern	cvar_t	*gl_modulate;
//extern	cvar_t	*gl_playermip;
extern	cvar_t	*gl_drawbuffer;
//extern	cvar_t	*gl_3dlabs_broken;
extern  cvar_t  *gl_driver;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_texturealphamode;
extern	cvar_t	*gl_texturesolidmode;
//extern  cvar_t  *gl_saturatelighting;
extern  cvar_t  *gl_lockpvs;

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;

extern	cvar_t	*gl_jpg_quality;
extern	cvar_t	*gl_coloredlightmaps;

extern	cvar_t	*intensity;

extern	cvar_t	*gl_dlight_falloff;
extern	cvar_t	*gl_alphaskins;
extern	cvar_t	*gl_defertext;

extern	cvar_t	*gl_pic_scale;

extern	cvar_t	*vid_restore_on_switch;

extern int		usingmodifiedlightmaps;

extern	int		defer_drawing;

extern	const int		gl_solid_format;
extern	const int		gl_alpha_format;
extern	int		gl_tex_solid_format;
extern	int		gl_tex_alpha_format;

extern	int		global_hax_texture_x;
extern	int		global_hax_texture_y;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	float	r_world_matrix[16];

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (unsigned int texnum);
void GL_MBind( GLenum target, unsigned int texnum );
void GL_TexEnv( GLenum value );
void GL_EnableMultitexture( qboolean enable );
void GL_SelectTexture( GLenum );

void R_LightPoint (vec3_t p, vec3_t color);
void R_PushDlights (void);
unsigned int hashify (const char *S);
//====================================================================

extern	model_t	*r_worldmodel;

extern	unsigned	d_8to24table[256];
extern	vec4_t		d_8to24float[256];

extern	int		registration_sequence;


void V_AddBlend (float r, float g, float b, float a, float *v_blend);

int 	EXPORT R_Init( void *hinstance, void *hWnd );
void	EXPORT R_Shutdown( void );

void R_RenderView (refdef_t *fd);
void GL_ScreenShot_f (void);
void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);
void R_DrawBeam( entity_t *e );
void R_DrawWorld (void);
void R_RenderDlights (void);
void R_DrawAlphaSurfaces (void);
void R_RenderBrushPoly (msurface_t *fa);
void R_InitParticleTexture (void);
void Draw_InitLocal (void);
void GL_SubdivideSurface (msurface_t *fa);
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_RotateForEntity (entity_t *e);
void R_MarkLeaves (void);

glpoly_t *WaterWarpPolyVerts (glpoly_t *p);
void EmitWaterPolys (msurface_t *fa);
void R_AddSkySurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);

#if 0
short LittleShort (short l);
short BigShort (short l);
int	LittleLong (int l);
float LittleFloat (float f);

char	*va(char *format, ...);
// does a varargs printf into a temp buffer
#endif

//void COM_StripExtension (char *in, char *out);

void	EXPORT Draw_GetPicSize (int *w, int *h, char *name);
void	EXPORT Draw_Pic (int x, int y, char *name);
void	EXPORT Draw_StretchPic (int x, int y, int w, int h, char *name);
void	EXPORT Draw_Char (int x, int y, int c);
void	EXPORT Draw_TileClear (int x, int y, int w, int h, char *name);
void	EXPORT Draw_Fill (int x, int y, int w, int h, int c);
void	EXPORT Draw_FadeScreen (void);
void	EXPORT Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void	EXPORT R_BeginFrame( float camera_separation );
void	EXPORT R_SetPalette ( const unsigned char *palette);

int		Draw_GetPalette (void);

void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight);

struct image_s * EXPORT R_RegisterSkin (char *name);

void LoadPCX (const char *filename, byte **pic, byte **palette, int *width, int *height);
image_t *GL_LoadPic (const char *name, byte *pic, int width, int height, imagetype_t type, int bits);
image_t	*GL_FindImage (const char *name, const char *basename, imagetype_t type);
image_t	*GL_FindImageBase (const char *basename, imagetype_t type);
void	GL_TextureMode( char *string );
void	GL_ImageList_f (void);
void	GL_Version_f (void);

//void	GL_SetTexturePalette( unsigned palette[256] );

void	GL_InitImages (void);
void	GL_ShutdownImages (void);

void	GL_FreeUnusedImages (void);

void GL_TextureAlphaMode( char *string );
void GL_TextureSolidMode( char *string );

/*
** GL extension emulation functions
*/
void GL_DrawParticles( int n, const particle_t particles[] );

void EmptyImageCache (void);

/*
** GL config stuff
*/
#define GL_RENDERER_VOODOO		0x00000001
#define GL_RENDERER_VOODOO2   	0x00000002
#define GL_RENDERER_VOODOO_RUSH	0x00000004
#define GL_RENDERER_BANSHEE		0x00000008
#define		GL_RENDERER_3DFX		0x0000000F

#define GL_RENDERER_PCX1		0x00000010
#define GL_RENDERER_PCX2		0x00000020
#define GL_RENDERER_PMX			0x00000040
#define		GL_RENDERER_POWERVR		0x00000070

#define GL_RENDERER_PERMEDIA2	0x00000100
#define GL_RENDERER_GLINT_MX	0x00000200
#define GL_RENDERER_GLINT_TX	0x00000400
#define GL_RENDERER_3DLABS_MISC	0x00000800
#define		GL_RENDERER_3DLABS	0x00000F00

#define GL_RENDERER_REALIZM		0x00001000
#define GL_RENDERER_REALIZM2	0x00002000
#define		GL_RENDERER_INTERGRAPH	0x00003000

#define GL_RENDERER_3DPRO		0x00004000
#define GL_RENDERER_REAL3D		0x00008000
#define GL_RENDERER_RIVA128		0x00010000
#define GL_RENDERER_DYPIC		0x00020000

#define GL_RENDERER_V1000		0x00040000
#define GL_RENDERER_V2100		0x00080000
#define GL_RENDERER_V2200		0x00100000
#define		GL_RENDERER_RENDITION	0x001C0000

#define GL_RENDERER_O2          0x00100000
#define GL_RENDERER_IMPACT      0x00200000
#define GL_RENDERER_RE			0x00400000
#define GL_RENDERER_IR			0x00800000
#define		GL_RENDERER_SGI			0x00F00000

#define GL_RENDERER_MCD			0x01000000
#define GL_RENDERER_ATI			0x02000000
#define GL_RENDERER_NV			0x04000000

#define GL_RENDERER_OTHER		0x80000000

//r1ch: my super leet gl extensions!
#define GL_GENERATE_MIPMAP_SGIS			0x8191
#define	GL_GENERATE_MIPMAP_HINT_SGIS	0x8192
#define GL_TEXTURE_MAX_ANISOTROPY_EXT   0x84FE

#define GL_MULTISAMPLE_ARB                0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE_ARB   0x809E
#define GL_SAMPLE_ALPHA_TO_ONE_ARB        0x809F
#define GL_SAMPLE_COVERAGE_ARB            0x80A0
#define GL_SAMPLE_BUFFERS_ARB             0x80A8
#define GL_SAMPLES_ARB                    0x80A9
#define GL_SAMPLE_COVERAGE_VALUE_ARB      0x80AA
#define GL_SAMPLE_COVERAGE_INVERT_ARB     0x80AB
#define GL_MULTISAMPLE_BIT_ARB            0x20000000

/* NV_multisample_filter_hint */
#define GL_MULTISAMPLE_FILTER_HINT_NV     0x8534

typedef struct
{
	int         renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	//qboolean	r1gl_GL_SGIS_generate_mipmap;
	qboolean	r1gl_GL_ARB_point_sprite;
	qboolean	r1gl_GL_EXT_texture_filter_anisotropic;
	qboolean	r1gl_GL_EXT_nv_multisample_filter_hint;
	qboolean	r1gl_GL_ARB_texture_non_power_of_two;
	qboolean	wglPFD;

	int			bitDepth;
	int			r1gl_QueryBits;
	unsigned int r1gl_Queries[MAX_ENTITIES];
} glconfig_t;

typedef struct
{
	float inverse_intensity;
	qboolean fullscreen;

	int     prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	unsigned	currenttextures[2];
	unsigned int currenttmu;
	GLenum currenttarget;

#ifdef STEREO_SUPPORT
	float camera_separation;
	qboolean stereo_enabled;
#endif

	unsigned char originalRedGammaTable[256];
	unsigned char originalGreenGammaTable[256];
	unsigned char originalBlueGammaTable[256];

	qboolean hwgamma;
} glstate_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

/*
====================================================================

IMPORTED FUNCTIONS

====================================================================
*/

extern	refimport_t		ri;
extern	refimportnew_t	rx;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

#ifdef STEREO_SUPPORT
void		GLimp_BeginFrame( float camera_separation );
#else
void		GLimp_BeginFrame( void );
#endif
void	EXPORT	GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
int    	GLimp_SetMode( unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen );
void	EXPORT	GLimp_AppActivate( qboolean active );
void		GLimp_EnableLogging( qboolean enable );
void		GLimp_LogNewFrame( void );


void GL_CheckForError (void);
