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

#include "gl_local.h"
#include <png.h>
#include <jpeglib.h>

image_t		gltextures[MAX_GLTEXTURES];
int			numgltextures = 0;
//int			base_textureid;		// gltextures[i] = base_textureid+i

static	byte			intensitytable[256];
static	byte			gammatable[256];
static	byte			gammaintensitytable[256];

cvar_t		*intensity;

unsigned	d_8to24table[256];

qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, image_t *image);
qboolean GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, int bpp, image_t *image);

char	*current_texture_filename;

int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_tex_solid_format = 3;
int		gl_tex_alpha_format = 4;

//int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_max = GL_LINEAR;

static void	*scaled_buffer = NULL;

#ifdef RB_IMAGE_CACHE

struct	rbtree *rb;

/*static int _compare(const void *pa, const void *pb, const void *config)
{
	return strcmp ((const char *)pa, (const char *)pb);
}*/

void DestroyImageCache (void)
{
	RBLIST	*rblist;
	const void	*val;

	if ((rblist=rbopenlist(rb)))
	{
		while((val=rbreadlist(rblist)))
			rbdelete (val, rb);
	}

	rbcloselist(rblist);	

	rbdestroy (rb);
}

void EmptyImageCache (void)
{
	RBLIST	*rblist;
	const void	*val;

	if ((rblist=rbopenlist(rb)))
	{
		while((val=rbreadlist(rblist)))
			rbdelete (val, rb);
	}

	rbcloselist(rblist);
}

#endif

/*void GL_SetTexturePalette( unsigned palette[256] )
{
	int i;
	unsigned char temptable[768];

	if ( qglColorTableEXT && gl_ext_palettedtexture->value )
	{
		for ( i = 0; i < 256; i++ )
		{
			temptable[i*3+0] = ( palette[i] >> 0 ) & 0xff;
			temptable[i*3+1] = ( palette[i] >> 8 ) & 0xff;
			temptable[i*3+2] = ( palette[i] >> 16 ) & 0xff;
		}

		qglColorTableEXT( GL_SHARED_TEXTURE_PALETTE_EXT,
						   GL_RGB,
						   256,
						   GL_RGB,
						   GL_UNSIGNED_BYTE,
						   temptable );
	}
}*/

void GL_EnableMultitexture( qboolean enable )
{
	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	GL_SelectTexture( GL_TEXTURE1 );

	if ( enable )
		qglEnable( GL_TEXTURE_2D );
	else
		qglDisable( GL_TEXTURE_2D );	

	GL_TexEnv( GL_REPLACE );

	GL_SelectTexture( GL_TEXTURE0 );
	GL_TexEnv( GL_REPLACE );
}

void GL_SelectTexture( GLenum texture )
{
	if (texture == gl_state.currenttarget) {
		return;
	} else {
		unsigned tmu;

		if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
			return;

		if ( texture == GL_TEXTURE0 )
		{
			tmu = 0;
		}
		else
		{
			tmu = 1;
		}

		gl_state.currenttmu = tmu;
		gl_state.currenttarget = texture;

		if ( qglSelectTextureSGIS )
		{
			qglSelectTextureSGIS( texture );
		}
		else if ( qglActiveTextureARB )
		{
			qglActiveTextureARB( texture );
			qglClientActiveTextureARB( texture );
		}
	}
}

void GL_TexEnv( GLenum mode )
{
	static GLenum lastmodes[2] = { -1, -1 };

	if ( mode != lastmodes[gl_state.currenttmu] )
	{
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		lastmodes[gl_state.currenttmu] = mode;
	}
}

void GL_Bind (unsigned int texnum)
{
#ifdef _DEBUG
	extern	image_t	*draw_chars;

	if (FLOAT_NE_ZERO(gl_nobind->value)) {
		if (gl_nobind->value == 2) {
			texnum = TEXNUM_SCRAPS;
		} else {
			if (draw_chars)		// performance evaluation option
				texnum = draw_chars->texnum;
		}
	}
#endif
	if ( gl_state.currenttextures[gl_state.currenttmu] == texnum)
		return;

	gl_state.currenttextures[gl_state.currenttmu] = texnum;

	qglBindTexture (GL_TEXTURE_2D, texnum);
}

void GL_MBind( GLenum target, unsigned int texnum )
{
	GL_SelectTexture( target );
	if ( target == GL_TEXTURE0 )
	{
		if ( gl_state.currenttextures[0] == texnum )
			return;
	}
	else
	{
		if ( gl_state.currenttextures[1] == texnum )
			return;
	}
	GL_Bind( texnum );
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] =
{
	{"GL_NEAREST",					GL_NEAREST,					GL_NEAREST},
	{"GL_LINEAR",					GL_LINEAR,					GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST",	GL_NEAREST_MIPMAP_NEAREST,	GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST",	GL_LINEAR_MIPMAP_NEAREST,	GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR",	GL_NEAREST_MIPMAP_LINEAR,	GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR",		GL_LINEAR_MIPMAP_LINEAR,	GL_LINEAR},
	//{"GL_LINEAR_MIPMAP_DETAIL",		GL_LINEAR_MIPMAP_LINEAR,	GL_LINEAR_DETAIL_SGIS},
	//{"GL_LINEAR_MIPMAP_SHARPEN",	GL_LINEAR_MIPMAP_LINEAR,	GL_LINEAR_SHARPEN_SGIS}
};

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

typedef struct
{
	char *name;
	int mode;
} gltmode_t;

gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( char *string )
{
	int		i;
	image_t	*glt;

	for (i=0 ; i< NUM_GL_MODES ; i++)
	{
		if ( !Q_stricmp( modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky )
		{
			GL_Bind (glt->texnum);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
GL_TextureAlphaMode
===============
*/
void GL_TextureAlphaMode( char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_ALPHA_MODES ; i++)
	{
		if ( !Q_stricmp( gl_alpha_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_ALPHA_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

/*
===============
GL_TextureSolidMode
===============
*/
void GL_TextureSolidMode( char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_SOLID_MODES ; i++)
	{
		if ( !Q_stricmp( gl_solid_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_SOLID_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}


/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up inefficient hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		1
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT];
qboolean	scrap_dirty;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	unsigned int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	return -1;
//	Sys_Error ("Scrap_AllocBlock: full");
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	scrap_uploads++;
	GL_Bind(TEXNUM_SCRAPS);
	GL_Upload8 (scrap_texels[0], BLOCK_WIDTH, BLOCK_HEIGHT, false, NULL);
	scrap_dirty = false;
}

/*
=================================================================

PCX LOADING

=================================================================
*/


/*
==============
LoadPCX
==============
*/
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		picSize;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	//
	// load the file
	//
	len = ri.FS_LoadFile (filename, (void **)&raw);
	if (!raw)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
		return;
	}

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

    pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		ri.Con_Printf (PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	picSize = (pcx->ymax+1) * (pcx->xmax+1);

	out = malloc ( picSize );

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = malloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	//if (strstr (filename, "loading"))
	//	DEBUGBREAKPOINT;

	if (width)
		*width = pcx->xmax+1;

	if (height)
		*height = pcx->ymax+1;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				pix[x++] = dataByte;
				if (x > pcx->xmax)
				{
					if (runLength)
						ri.Con_Printf (PRINT_DEVELOPER, "WARNING: PCX file %s: runlength exceeds width (%d bytes still in run)\n", filename, runLength);
					break;
				}
			}
		}
	}

	if ( raw - (byte *)pcx > len)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "PCX file %s was malformed", filename);
		free (*pic);
		*pic = NULL;
	}

	ri.FS_FreeFile (pcx);
}

typedef struct {
    byte *Buffer;
    size_t Pos;
} TPngFileBuffer;

void __cdecl PngReadFunc(png_struct *Png, png_bytep buf, png_size_t size)
{
    TPngFileBuffer *PngFileBuffer=(TPngFileBuffer*)png_get_io_ptr(Png);
    memcpy(buf,PngFileBuffer->Buffer+PngFileBuffer->Pos,size);
    PngFileBuffer->Pos+=size;
}


void LoadPNG (char *name, byte **pic, int *width, int *height)
{
	unsigned int	i, rowbytes;
	png_structp		png_ptr;
	png_infop		info_ptr;
	png_infop		end_info;
	png_bytep		row_pointers[MAX_TEXTURE_DIMENSIONS];
	double			file_gamma;

	TPngFileBuffer	PngFileBuffer = {NULL,0};

	*pic = NULL;

	ri.FS_LoadFile (name, (void *)&PngFileBuffer.Buffer);

    if (!PngFileBuffer.Buffer)
		return;

	if ((png_check_sig(PngFileBuffer.Buffer, 8)) == 0)
	{
		ri.FS_FreeFile (PngFileBuffer.Buffer); 
		ri.Con_Printf (PRINT_ALL, "Not a PNG file: %s\n", name);
		return;
    }

	PngFileBuffer.Pos=0;

    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL,  NULL, NULL);

    if (!png_ptr)
	{
		ri.FS_FreeFile (PngFileBuffer.Buffer);
		ri.Con_Printf (PRINT_ALL, "Bad PNG file: %s\n", name);
		return;
	}

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
	{
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		ri.FS_FreeFile (PngFileBuffer.Buffer);
		ri.Con_Printf (PRINT_ALL, "Bad PNG file: %s\n", name);
		return;
    }
    
	end_info = png_create_info_struct(png_ptr);
    if (!end_info)
	{
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		ri.FS_FreeFile (PngFileBuffer.Buffer);
		ri.Con_Printf (PRINT_ALL, "Bad PNG file: %s\n", name);
		return;
    }

	png_set_read_fn (png_ptr,(png_voidp)&PngFileBuffer,(png_rw_ptr)PngReadFunc);

	png_read_info(png_ptr, info_ptr);

	if (info_ptr->height > MAX_TEXTURE_DIMENSIONS)
	{
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		ri.FS_FreeFile (PngFileBuffer.Buffer);
		ri.Con_Printf (PRINT_ALL, "Oversized PNG file: %s\n", name);
		return;
	}

	if (info_ptr->color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_palette_to_rgb (png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}

	if (info_ptr->color_type == PNG_COLOR_TYPE_RGB)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if ((info_ptr->color_type == PNG_COLOR_TYPE_GRAY) && info_ptr->bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (info_ptr->color_type == PNG_COLOR_TYPE_GRAY || info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (info_ptr->bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (info_ptr->bit_depth < 8)
        png_set_packing(png_ptr);

	if (png_get_gAMA(png_ptr, info_ptr, &file_gamma))
		png_set_gamma (png_ptr, 2.0, file_gamma);

	png_read_update_info(png_ptr, info_ptr);

	rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	*pic = malloc (info_ptr->height * rowbytes);

	for (i = 0; i < info_ptr->height; i++)
		row_pointers[i] = *pic + i*rowbytes;

	png_read_image(png_ptr, row_pointers);

	*width = info_ptr->width;
	*height = info_ptr->height;

	png_read_end(png_ptr, end_info);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	ri.FS_FreeFile (PngFileBuffer.Buffer);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader {
	unsigned char id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char pixel_size, attributes;
} TargaHeader;

// Definitions for image types
#define TGA_Null		0	// no image data
#define TGA_Map			1	// Uncompressed, color-mapped images
#define TGA_RGB			2	// Uncompressed, RGB images
#define TGA_Mono		3	// Uncompressed, black and white images
#define TGA_RLEMap		9	// Runlength encoded color-mapped images
#define TGA_RLERGB		10	// Runlength encoded RGB images
#define TGA_RLEMono		11	// Compressed, black and white images
#define TGA_CompMap		32	// Compressed color-mapped data, using Huffman, Delta, and runlength encoding
#define TGA_CompMap4	33	// Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process
// Definitions for interleave flag
#define TGA_IL_None		0	// non-interleaved
#define TGA_IL_Two		1	// two-way (even/odd) interleaving
#define TGA_IL_Four		2	// four way interleaving
#define TGA_IL_Reserved	3	// reserved
// Definitions for origin flag
#define TGA_O_UPPER		0	// Origin in lower left-hand corner
#define TGA_O_LOWER		1	// Origin in upper left-hand corner
#define MAXCOLORS 16384

/*
=============
LoadTGA
NiceAss: LoadTGA() from Q2Ice, it supports more formats
=============
*/
void LoadTGA (char *filename, byte **pic, int *width, int *height)
{
	int			w, h, x, y, i, temp1, temp2;
	int			realrow, truerow, baserow, size, interleave, origin;
	int			pixel_size, map_idx, mapped, rlencoded, RLE_count, RLE_flag;
	TargaHeader	header;
	byte		tmp[2], r, g, b, a, j, k, l;
	byte		*dst, *ColorMap, *data, *pdata;

	*pic = NULL;

	// load file
	ri.FS_LoadFile( filename, (void *)&data );

	if( !data )
		return;

	pdata = data;

	header.id_length = *pdata++;
	header.colormap_type = *pdata++;
	header.image_type = *pdata++;
	
	tmp[0] = pdata[0];
	tmp[1] = pdata[1];
	header.colormap_index = LittleShort( *((short *)tmp) );
	pdata+=2;
	tmp[0] = pdata[0];
	tmp[1] = pdata[1];
	header.colormap_length = LittleShort( *((short *)tmp) );
	pdata+=2;
	header.colormap_size = *pdata++;
	header.x_origin = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.y_origin = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.width = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.height = LittleShort( *((short *)pdata) );
	pdata+=2;
	header.pixel_size = *pdata++;
	header.attributes = *pdata++;

	if( header.id_length )
		pdata += header.id_length;

	// validate TGA type
	switch( header.image_type ) {
		case TGA_Map:
		case TGA_RGB:
		case TGA_Mono:
		case TGA_RLEMap:
		case TGA_RLERGB:
		case TGA_RLEMono:
			break;
		default:
			ri.Sys_Error ( ERR_DROP, "LoadTGA: (%s): Only type 1 (map), 2 (RGB), 3 (mono), 9 (RLEmap), 10 (RLERGB), 11 (RLEmono) TGA images supported\n", filename);
			return;
	}

	// validate color depth
	switch( header.pixel_size ) {
		case 8:
		case 15:
		case 16:
		case 24:
		case 32:
			break;
		default:
			ri.Sys_Error ( ERR_DROP, "LoadTGA: (%s): Only 8, 15, 16, 24 and 32 bit images (with colormaps) supported\n", filename);
			return;
	}

	r = g = b = a = l = 0;

	// if required, read the color map information
	ColorMap = NULL;
	mapped = ( header.image_type == TGA_Map || header.image_type == TGA_RLEMap || header.image_type == TGA_CompMap || header.image_type == TGA_CompMap4 ) && header.colormap_type == 1;
	if( mapped ) {
		// validate colormap size
		switch( header.colormap_size ) {
			case 8:
			case 16:
			case 32:
			case 24:
				break;
			default:
				ri.Sys_Error ( ERR_DROP, "LoadTGA: (%s): Only 8, 16, 24 and 32 bit colormaps supported\n", filename);
				return;
		}

		temp1 = header.colormap_index;
		temp2 = header.colormap_length;
		if( (temp1 + temp2 + 1) >= MAXCOLORS ) {
			ri.FS_FreeFile( data );
			return;
		}
		ColorMap = (byte *)malloc( MAXCOLORS * 4 );
		map_idx = 0;
		for( i = temp1; i < temp1 + temp2; ++i, map_idx += 4 ) {
			// read appropriate number of bytes, break into rgb & put in map
			switch( header.colormap_size ) {
				case 8:
					r = g = b = *pdata++;
					a = 255;
					break;
				case 15:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = (k & 0x80) ? 255 : 0;
					break;
				case 24:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = 255;
					l = 0;
					break;
				case 32:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = *pdata++;
					l = 0;
					break;
			}
			ColorMap[map_idx + 0] = r;
			ColorMap[map_idx + 1] = g;
			ColorMap[map_idx + 2] = b;
			ColorMap[map_idx + 3] = a;
		}
	}

	// check run-length encoding
	rlencoded = header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono;
	RLE_count = 0;
	RLE_flag = 0;

	w = header.width;
	h = header.height;

	if( width )
		*width = w;
	if( height )
		*height = h;

	size = w * h * 4;
	*pic = (byte *)malloc( size );

	memset( *pic, 0, size );

	// read the Targa file body and convert to portable format
	pixel_size = header.pixel_size;
	origin = (header.attributes & 0x20) >> 5;
	interleave = (header.attributes & 0xC0) >> 6;
	truerow = 0;
	baserow = 0;
	for( y = 0; y < h; y++ ) {
		realrow = truerow;
		if( origin == TGA_O_UPPER )
			realrow = h - realrow - 1;

		dst = *pic + realrow * w * 4;

		for( x = 0; x < w; x++ ) {
			// check if run length encoded
			if( rlencoded ) {
				if( !RLE_count ) {
					// have to restart run
					i = *pdata++;
					RLE_flag = (i & 0x80);
					if( !RLE_flag ) {
						// stream of unencoded pixels
						RLE_count = i + 1;
					} else {
						// single pixel replicated
						RLE_count = i - 127;
					}
					// decrement count & get pixel
					--RLE_count;
				} else {
					// have already read count & (at least) first pixel
					--RLE_count;
					if( RLE_flag )
						// replicated pixels
						goto PixEncode;
				}
			}

			// read appropriate number of bytes, break into RGB
			switch( pixel_size ) {
				case 8:
					r = g = b = l = *pdata++;
					a = 255;
					break;
				case 15:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 16:
					j = *pdata++;
					k = *pdata++;
					l = ((unsigned int) k << 8) + j;
					r = (byte) ( ((k & 0x7C) >> 2) << 3 );
					g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 );
					b = (byte) ( (j & 0x1F) << 3 );
					a = 255;
					break;
				case 24:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = 255;
					l = 0;
					break;
				case 32:
					b = *pdata++;
					g = *pdata++;
					r = *pdata++;
					a = *pdata++;
					l = 0;
					break;
				default:
					ri.Sys_Error( ERR_DROP, "LoadTGA: (%s): Illegal pixel_size '%d'", filename, pixel_size );
					return;
			}

PixEncode:
			if ( mapped )
			{
				map_idx = l * 4;
				*dst++ = ColorMap[map_idx + 0];
				*dst++ = ColorMap[map_idx + 1];
				*dst++ = ColorMap[map_idx + 2];
				*dst++ = ColorMap[map_idx + 3];
			}
			else
			{
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		if (interleave == TGA_IL_Four)
			truerow += 4;
		else if (interleave == TGA_IL_Two)
			truerow += 2;
		else
			truerow++;

		if (truerow >= h)
			truerow = ++baserow;
	}

	if (mapped)
		free( ColorMap );
	
	ri.FS_FreeFile( data );
}

/*
=================================================================

JPEG LOADING
NiceAss: Code from Q2Ice

=================================================================
*/

void __cdecl jpg_null(j_decompress_ptr cinfo)
{
}

unsigned char __cdecl jpg_fill_input_buffer(j_decompress_ptr cinfo)
{
    ri.Con_Printf(PRINT_ALL, "Premature end of JPEG data\n");
    return 1;
}

void __cdecl jpg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
        
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

void jpeg_mem_src(j_decompress_ptr cinfo, byte *mem, int len)
{
    cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
    cinfo->src->init_source = jpg_null;
    cinfo->src->fill_input_buffer = jpg_fill_input_buffer;
    cinfo->src->skip_input_data = jpg_skip_input_data;
    cinfo->src->resync_to_restart = jpeg_resync_to_restart;
    cinfo->src->term_source = jpg_null;
    cinfo->src->bytes_in_buffer = len;
    cinfo->src->next_input_byte = mem;
}

/*
==============
LoadJPG
==============
*/
void LoadJPG (char *filename, byte **pic, int *width, int *height)
{
	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr			jerr;
	byte							*rawdata, *rgbadata, *scanline, *p, *q;
	unsigned int					rawsize, i;

	*pic = NULL;

	// Load JPEG file into memory
	rawsize = ri.FS_LoadFile(filename, (void **)&rawdata);

	if(!rawdata)
		return;	

	if ( rawdata[6] != 'J' || rawdata[7] != 'F' || rawdata[8] != 'I' || rawdata[9] != 'F')
	{ 
		ri.Con_Printf (PRINT_ALL, "Invalid JPEG header: %s\n", filename); 
		ri.FS_FreeFile(rawdata); 
		return; 
	} 

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, rawdata, rawsize);
	jpeg_read_header(&cinfo, true);
	jpeg_start_decompress(&cinfo);

	if(cinfo.output_components != 3 && cinfo.output_components != 4)
	{
		ri.Con_Printf(PRINT_ALL, "Invalid JPEG colour components\n");
		jpeg_destroy_decompress(&cinfo);
		ri.FS_FreeFile(rawdata);
		return;
	}

	// Allocate Memory for decompressed image
	rgbadata = malloc(cinfo.output_width * cinfo.output_height * 4);
	if(!rgbadata)
	{
		ri.Con_Printf(PRINT_ALL, "Insufficient memory for JPEG buffer\n");
		jpeg_destroy_decompress(&cinfo);
		ri.FS_FreeFile(rawdata);
		return;
	}

	// Pass sizes to output
	*width = cinfo.output_width; *height = cinfo.output_height;

	// Allocate Scanline buffer
	scanline = malloc (cinfo.output_width * 3);
	if (!scanline)
	{
		ri.Con_Printf (PRINT_ALL, "Insufficient memory for JPEG scanline buffer\n");
		free (rgbadata);
		jpeg_destroy_decompress (&cinfo);
		ri.FS_FreeFile (rawdata);
		return;
	}

	// Read Scanlines, and expand from RGB to RGBA
	q = rgbadata;
	while (cinfo.output_scanline < cinfo.output_height)
	{
		p = scanline;
		jpeg_read_scanlines(&cinfo, &scanline, 1);

		for (i = 0; i < cinfo.output_width; i++)
		{
			q[0] = p[0];
			q[1] = p[1];
			q[2] = p[2];
			q[3] = 255;
			p += 3;
			q += 4;
		}
	}

	free (scanline);
	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);

	*pic = rgbadata;
}


/*typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

typedef struct targaHeader_s {
	byte			idLength;
	byte			colorMapType;
	byte			imageType;

	unsigned short	colorMapIndex;
	unsigned short	colorMapLength;
	byte			colorMapSize;

	unsigned short	xOrigin;
	unsigned short	yOrigin;
	unsigned short	width;
	unsigned short	height;

	byte			pixelSize;

	byte			attributes;
} targaHeader_t;*/

/*
=============
LoadTGA
=============
*/
/*void LoadTGA (char *name, byte **pic, int *width, int *height)
{
	int				i, columns, rows, row_inc, row, col;
	byte			*buf_p, *buffer, *pixbuf, *targa_rgba;
	int				length, samples, readpixelcount, pixelcount;
	byte			palette[256][4], red, green, blue, alpha;
	qboolean		compressed;
	targaHeader_t	targa_header;

	*pic = NULL;

	//
	// load the file
	//

	length = ri.FS_LoadFile (name, (void **)&buffer);
	if (!buffer || (length <= 0))
		return;

	buf_p = buffer;
	targa_header.idLength = *buf_p++;
	targa_header.colorMapType = *buf_p++;
	targa_header.imageType = *buf_p++;

	targa_header.colorMapIndex = buf_p[0] + buf_p[1] * 256;
	buf_p+=2;
	targa_header.colorMapLength = buf_p[0] + buf_p[1] * 256;
	buf_p+=2;
	targa_header.colorMapSize = *buf_p++;
	targa_header.xOrigin = LittleShort (*((short *)buf_p));
	buf_p+=2;
	targa_header.yOrigin = LittleShort (*((short *)buf_p));
	buf_p+=2;
	targa_header.width = LittleShort (*((short *)buf_p));
	buf_p+=2;
	targa_header.height = LittleShort (*((short *)buf_p));
	buf_p+=2;
	targa_header.pixelSize = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.idLength != 0)
		buf_p += targa_header.idLength;  // skip TARGA image comment

	if ((targa_header.imageType == 1) || (targa_header.imageType == 9))
	{
		// uncompressed colormapped image
		if (targa_header.pixelSize != 8)
		{
			ri.FS_FreeFile (buffer);
			ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only 8 bit images supported for type 1 and 9\n", name);
		}

		if( targa_header.colorMapLength != 256 )
		{
			ri.FS_FreeFile (buffer);
			ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only 8 bit colormaps are supported for type 1 and 9\n", name);
		}

		if( targa_header.colorMapIndex )
		{
			ri.FS_FreeFile (buffer);
			ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) colorMapIndex is not supported for type 1 and 9", name);
		}

		if (targa_header.colorMapSize == 24)
		{
			for (i=0 ; i<targa_header.colorMapLength ; i++)
			{
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = 255;
			}
		}
		else if (targa_header.colorMapSize == 32)
		{
			for (i=0 ; i<targa_header.colorMapLength ; i++)
			{
				palette[i][0] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][2] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		}
		else
		{
			ri.FS_FreeFile (buffer);
			ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only 24 and 32 bit colormaps are supported for type 1 and 9", name);
		}
	}
	else if ((targa_header.imageType == 2) || (targa_header.imageType == 10))
	{
		// uncompressed or RLE compressed RGB
		if ((targa_header.pixelSize != 32) && (targa_header.pixelSize != 24))
		{
			ri.FS_FreeFile (buffer);
			ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only 32 or 24 bit images supported for type 2 and 10", name);
		}
	}
	else if ((targa_header.imageType == 3) || (targa_header.imageType == 11))
	{
		// uncompressed greyscale
		if (targa_header.pixelSize != 8 )
		{
			ri.FS_FreeFile (buffer);
			ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only 8 bit images supported for type 3 and 11", name);
		}
	}

	columns = targa_header.width;
	if (width)
		*width = columns;

	rows = targa_header.height;
	if (height)
		*height = rows;

	targa_rgba = malloc (columns * rows * 4);
	*pic = targa_rgba;

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if (targa_header.attributes & 0x20)
	{
		pixbuf = targa_rgba;
		row_inc = 0;
	}
	else
	{
		pixbuf = targa_rgba + (rows - 1) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	compressed = ((targa_header.imageType == 9) || (targa_header.imageType == 10) || (targa_header.imageType == 11));

	for (row=col=0, samples=3 ; row<rows ; )
	{
		pixelcount = 0x10000;
		readpixelcount = 0x10000;

		if (compressed)
		{
			pixelcount = *buf_p++;
			if (pixelcount & 0x80)	// run-length packet
				readpixelcount = 1;
			pixelcount = 1 + (pixelcount & 0x7f);
		}

		while (pixelcount-- && (row < rows))
		{
			if (readpixelcount-- > 0)
			{
				switch (targa_header.imageType)
				{
					case 1:
					case 9:
						// colormapped image
						blue = *buf_p++;
						red = palette[blue][0];
						green = palette[blue][1];
						alpha = palette[blue][3];
						blue = palette[blue][2];
						if (alpha != 255)
							samples = 4;
						break;
					case 2:
					case 10:
						// 24 or 32 bit image
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alpha = 255;
						if (targa_header.pixelSize == 32) {
							alpha = *buf_p++;
							if (alpha != 255)
								samples = 4;
						}
						break;
					case 3:
					case 11:
						// greyscale image
						blue = green = red = *buf_p++;
						alpha = 255;
						break;
				}
			}

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;

			// run spans across rows
			if (++col == columns)
			{
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}

	ri.FS_FreeFile (buffer);
}*/

/*void LoadTGA (char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	byte	*buffer;
	//int		length;
	TargaHeader		targa_header;
	byte			*targa_rgba;
	byte tmp[2];

	*pic = NULL;

	//
	// load the file
	//
	ri.FS_LoadFile (name, (void **)&buffer);
	if (!buffer)
		return;

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort ( *((short *)tmp) );
	buf_p+=2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.y_origin = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.width = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.height = LittleShort ( *((short *)buf_p) );
	buf_p+=2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type!=2 
		&& targa_header.image_type!=10) 
		ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only type 2 and 10 targa RGB images supported\n", name);

	if (targa_header.colormap_type !=0 
		|| (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
		ri.Sys_Error (ERR_DROP, "LoadTGA: (%s) Only 32 or 24 bit images supported (no colormaps)\n", name);

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	targa_rgba = malloc (numPixels*4);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length;  // skip TARGA image comment
	
	if (targa_header.image_type==2) {  // Uncompressed, RGB images
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) {
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) {
					case 24:
							
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
					case 32:
							blue = *buf_p++;
							green = *buf_p++;
							red = *buf_p++;
							alphabyte = *buf_p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; ) {
				packetHeader= *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = 255;
								break;
						case 32:
								blue = *buf_p++;
								green = *buf_p++;
								red = *buf_p++;
								alphabyte = *buf_p++;
								break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = *buf_p++;
									green = *buf_p++;
									red = *buf_p++;
									alphabyte = *buf_p++;
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
						}
						column++;
						if (column==columns) { // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}

	ri.FS_FreeFile (buffer);
}*/


/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void R_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

//=======================================================

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[1024], p2[1024];
	//byte		noalpha[4] = {255,255,255,255};
	byte		*pix1, *pix2, *pix3, *pix4;

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i=0 ; i<outwidth ; i++)
	{
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}

	frac = 3*(fracstep>>2);
	for (i=0 ; i<outwidth ; i++)
	{
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(int)((i+0.25f)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75f)*inheight/outheight);

		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];

			((byte *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

void GL_ResampleTexture24(unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int i;
	byte *b1,*b2;
	unsigned *tmp,*tmp2;

	tmp=malloc(inheight*inwidth*4);
	tmp2=malloc(outwidth*outheight*4);

	b2=(byte*)tmp;
	b1=(byte*)in;

	for(i=0;i<inheight*inwidth;i++){    
		*b2++ = *b1++;
		*b2++ = *b1++;
		*b2++ = *b1++;
		*b2++ = 255;
	}

	GL_ResampleTexture(tmp,inwidth,inheight,tmp2,outwidth,outheight);

	b2=(byte*)out;
	b1=(byte*)tmp2;

	for(i=0;i<outheight*outwidth;i++){    
		*b2++ = *b1++;
		*b2++ = *b1++;
		*b2++ = *b1++;
		b1++;
	}

	free(tmp);
	free(tmp2);
}

/*
================
GL_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
void GL_LightScaleTexture (unsigned *in, int inwidth, int inheight, qboolean only_gamma)
{
	if ( only_gamma )
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=4)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=4)
		{
			p[0] = gammaintensitytable[p[0]];
			p[1] = gammaintensitytable[p[1]];
			p[2] = gammaintensitytable[p[2]];
		}
	}
}

void GL_LightScaleTexture24 (unsigned *in, int inwidth, int inheight, qboolean only_gamma)
{
	if ( only_gamma )
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=3)
		{
			p[0] = gammatable[p[0]];
			p[1] = gammatable[p[1]];
			p[2] = gammatable[p[2]];
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;
		for (i=0 ; i<c ; i++, p+=3)
		{
			p[0] = gammaintensitytable[p[0]];
			p[1] = gammaintensitytable[p[1]];
			p[2] = gammaintensitytable[p[2]];
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}

	//gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, in, width/4, height/4, GL_UNSIGNED_BYTE, in);
}

int		upload_width, upload_height;

qboolean GL_Upload32 (unsigned *data, int width, int height, qboolean mipmap, int bpp, image_t *image)
{
	int			samples;
	unsigned	*scaled = NULL;
	int			scaled_width, scaled_height;
	int			i, c;
	//byte		*scan;
	int comp;

	//if (strstr (current_texture_filename, "conback"))
	//	_asm int 3;

	if (gl_config.r1gl_GL_ARB_texture_non_power_of_two)
	{
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
			;
		if (FLOAT_NE_ZERO(gl_round_down->value) && scaled_width > width && mipmap)
			scaled_width >>= 1;
		for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
			;
		if (FLOAT_NE_ZERO(gl_round_down->value) && scaled_height > height && mipmap)
			scaled_height >>= 1;
	}

	// let people sample down the world textures for speed
	if (mipmap)
	{
		scaled_width >>= (int)gl_picmip->value;
		scaled_height >>= (int)gl_picmip->value;
	}

	// don't ever bother with >256 textures
	if (scaled_width > MAX_TEXTURE_DIMENSIONS)
		scaled_width = MAX_TEXTURE_DIMENSIONS;

	if (scaled_height > MAX_TEXTURE_DIMENSIONS)
		scaled_height = MAX_TEXTURE_DIMENSIONS;

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)

	upload_width = scaled_width;
	upload_height = scaled_height;

	//r1: why bother malloc/memcpy if its discarded?
	if (scaled_width == width && scaled_height == height)
	{
		scaled = data;
	}
	else
	{
		if (r_registering)
		{
			if (!scaled_buffer)
			{
				scaled_buffer = malloc(MAX_TEXTURE_DIMENSIONS * MAX_TEXTURE_DIMENSIONS * sizeof(int));
				if (!scaled_buffer)
					ri.Sys_Error (ERR_DROP, "GL_Upload32: %s: out of memory", current_texture_filename);
			}

			scaled = scaled_buffer;
		}
		else
		{
			scaled = malloc (scaled_width * scaled_height * sizeof(int));
			if (!scaled)
				ri.Sys_Error (ERR_DROP, "GL_Upload32: %s: out of memory", current_texture_filename);
		}
	}

	// scan the texture for any non-255 alpha
	samples = gl_solid_format;
	
	if (bpp == 8)
	{
		c = width*height;
		//scan = ((byte *)data) + 3;
		for (i=0 ; i<c ; i+= 4)
		{
			if (*(byte *)&data[i] != 255)
			{
				samples = gl_alpha_format;
				break;
			}
		}
	}
	else if (bpp == 32)
	{
		samples = gl_alpha_format;
	}

	if (samples == gl_solid_format)
	    comp = gl_tex_solid_format;
	else if (samples == gl_alpha_format)
	    comp = gl_tex_alpha_format;
	else {
	    ri.Con_Printf (PRINT_ALL,
			"GL_Upload32: %s: Unknown number of texture components %i\n",
			   current_texture_filename, samples);
	    comp = samples;
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		if (scaled != data)
			memcpy (scaled, data, width * height * sizeof(int));
	}
	else
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}

	if (image && (image->type != it_pic || FLOAT_NE_ZERO(vid_gamma_pics->value)))
		GL_LightScaleTexture (scaled, scaled_width, scaled_height, !mipmap );

	//r1ch: hardware/driver mipmap generation
	//0.1.5: removed due to shitty drivers breaking it, thx ati
	/*if (gl_config.r1gl_GL_SGIS_generate_mipmap)
	{
		qglTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		if ((err = qglGetError()) != GL_NO_ERROR) ri.Sys_Error (ERR_FATAL, "glGetError: 0x%x", err);

		qglTexParameteri (GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
		if ((err = qglGetError()) != GL_NO_ERROR) ri.Sys_Error (ERR_FATAL, "glGetError: 0x%x", err);
	}*/

	if (gl_config.r1gl_GL_EXT_texture_filter_anisotropic)
	{
		qglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ext_max_anisotropy->value);
	}

	qglTexImage2D (GL_TEXTURE_2D, 0, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

	//if (mipmap && !(gl_config.r1gl_GL_SGIS_generate_mipmap))
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, comp, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;


	if (mipmap) {
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	} else {
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
	}

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	if (!r_registering)
	{
		if (scaled && scaled != data)
			free (scaled);
	}

	return (samples == gl_alpha_format) ? true : false;
}

/*
===============
GL_Upload8

Returns has_alpha
===============
*/
/*
static qboolean IsPowerOf2( int value )
{
	int i = 1;


	while ( 1 )
	{
		if ( value == i )
			return true;
		if ( i > value )
			return false;
		i <<= 1;
	}
}
*/

qboolean GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, image_t *image)
{
	unsigned	trans[512*256];
	int			i, s;
	int			p;

	s = width*height;

	if (s > sizeof(trans)/4)
		ri.Sys_Error (ERR_DROP, "GL_Upload8: %s: %dx%d too large", current_texture_filename, width, height);

	for (i=0 ; i<s ; i++)
	{
		p = data[i];
		trans[i] = d_8to24table[p];

		if (p == 255)
		{	// transparent, so scan around for another color
			// to avoid alpha fringes
			// FIXME: do a full flood fill so mips work...
			if (i > width && data[i-width] != 255)
				p = data[i-width];
			else if (i < s-width && data[i+width] != 255)
				p = data[i+width];
			else if (i > 0 && data[i-1] != 255)
				p = data[i-1];
			else if (i < s-1 && data[i+1] != 255)
				p = data[i+1];
			else
				p = 0;
			// copy rgb components
			((byte *)&trans[i])[0] = ((byte *)&d_8to24table[p])[0];
			((byte *)&trans[i])[1] = ((byte *)&d_8to24table[p])[1];
			((byte *)&trans[i])[2] = ((byte *)&d_8to24table[p])[2];
		}
	}

	return GL_Upload32 (trans, width, height, mipmap, 8, image);
}


/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits)
{
	qboolean	mipmap;
	image_t		*image;
	int			i;

	// find a free image_t
	for (i=0, image=gltextures ; i<numgltextures ; i++,image++)
	{
		if (!image->texnum)
			break;
	}
	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
		{
			FILE	*dump;
			dump = fopen ("./gltextures.txt", "wb");
			if (dump)
			{
				for (i=0, image=gltextures ; i<numgltextures ; i++,image++)
				{
					fprintf (dump, "%i: %s[%s], %dx%d, texnum %u, type %d, sequence %d\n", i, image->basename, image->name, image->width, image->height, image->texnum, image->type, image->registration_sequence);
				}
				fclose (dump);
			}
			ri.Sys_Error (ERR_DROP, "MAX_GLTEXTURES");
		}
		numgltextures++;
	}
	image = &gltextures[i];

	if (strlen(name) >= sizeof(image->name)-1)
		ri.Sys_Error (ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);

	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;
	//image->scrap = false;

	if (type == it_skin)// && bits == 8)
		R_FloodFillSkin(pic, width, height);

	// load little pics into the scrap
	if (image->type == it_pic && image->width < 64 && image->height < 64 && FLOAT_EQ_ZERO(gl_noscrap->value))
	{
		//image->scrap = true;

		if (bits == 8)
		{
			int		x, y;
			int		i, j, k;
			int		temp;
			unsigned	int	texnum;

			temp = Scrap_AllocBlock (image->width, image->height, &x, &y);

			if (temp == -1)
				goto nonscrap;
			else
				texnum = temp;

			scrap_dirty = true;

			// copy the texels into the scrap block
			k = 0;
			for (i=0 ; i<image->height ; i++)
				for (j=0 ; j<image->width ; j++, k++)
					scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = pic[k];
			image->texnum = TEXNUM_SCRAPS + texnum;
			//image->scrap = true;
			image->has_alpha = true;

			image->upload_height = image->width;
			image->upload_height = image->height;

			image->sl = (x+0.01f)/(float)BLOCK_WIDTH;
			image->sh = (x+image->width-0.01f)/(float)BLOCK_WIDTH;

			image->tl = (y+0.01f)/(float)BLOCK_WIDTH;
			image->th = (y+image->height-0.01f)/(float)BLOCK_WIDTH;	

			return image;
		}
	}

nonscrap:
	image->texnum = TEXNUM_IMAGES + (image - gltextures);
	GL_Bind(image->texnum);

	mipmap = (image->type != it_pic && image->type != it_sky) ? true : false;

	if (bits == 8)
		image->has_alpha = GL_Upload8 (pic, width, height, mipmap, image);
	else
		image->has_alpha = GL_Upload32 ((unsigned *)pic, width, height, mipmap, bits, image);

	image->upload_width = upload_width;
	image->upload_height = upload_height;

	if (global_hax_texture_x && global_hax_texture_y)
	{
		image->width = global_hax_texture_x;
		image->height = global_hax_texture_y;
	}

	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

	return image;
}

/*
================
GL_LoadWal
================
*/
image_t *GL_LoadWal (char *name)
{
	miptex_t	*mt;
	int			width, height, ofs, len;
	image_t		*image;

	len = ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		ri.Con_Printf (PRINT_ALL, "GL_FindImage: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	if (ofs < sizeof(*mt) || ofs >= len)
		ri.Sys_Error (ERR_DROP, "Bad texture offset %d in %s", ofs, name);

	image = GL_LoadPic (name, (byte *)mt + ofs, width, height, it_wall, 8);

	ri.FS_FreeFile ((void *)mt);

	return image;
}

#define IMAGES_HASH_SIZE	64
static image_t	*images_hash[IMAGES_HASH_SIZE];

unsigned int hashify (char *S)
{
  unsigned int hash_PeRlHaSh;

  hash_PeRlHaSh = 0;
  
  while (*S)
  {
	  hash_PeRlHaSh = hash_PeRlHaSh * 33 + (*(S++));
  }
  return hash_PeRlHaSh + (hash_PeRlHaSh >> 5);
}

void Cmd_HashStats_f (void)
{
	int hash;
	image_t *imghash;

	for (hash = 0; hash < IMAGES_HASH_SIZE; hash++)
	{
		ri.Con_Printf (PRINT_ALL, "%d: ", hash);
		for(imghash = images_hash[hash]; imghash; imghash = imghash->hash_next)
		{
			ri.Con_Printf (PRINT_ALL, "*");
		}
		ri.Con_Printf (PRINT_ALL, "\n");
	}
}

image_t	*GL_FindImageBase (char *basename, imagetype_t type)
{
	image_t		*imghash;
	unsigned	hash;

	hash = hashify(basename) % IMAGES_HASH_SIZE;

	for(imghash = images_hash[hash]; imghash; imghash = imghash->hash_next)
	{
		if (imghash->type == type && !strcmp(imghash->basename, basename))
		{
			imghash->registration_sequence = registration_sequence;
			return imghash;
		}
	}

	return NULL;
}

/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
image_t	*GL_FindImage (char *name, char *basename, imagetype_t type)
{
	image_t	*image;
	image_t	*imghash;
	byte	*pic;
	byte	*palette;
	size_t	len;
	int		width, height, bpp;
	unsigned long hash;

	hash = hashify(basename) % IMAGES_HASH_SIZE;

	for(imghash = images_hash[hash]; imghash; imghash = imghash->hash_next)
	{
		if (imghash->type == type && !strcmp(imghash->name, name))
		{
			imghash->registration_sequence = registration_sequence;
			return imghash;
		}
	}

	//hash buckets are quicker than binary tree for smaller amount of items.
	/*image = rbfind (name, rb);	

	if (image)
	{
		image = *(image_t **)image;
		image->registration_sequence = registration_sequence;
		STOP_PERFORMANCE_TIMER;
		return image;
	}*/

	//if (strstr (name, "c_head"))
	//	_asm int 3;

	len = strlen(name);

	//if (len < 5)
	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: Bad image name: %s", name);

	//
	// load the pic from disk
	//
	pic = NULL;
	palette = NULL;
	current_texture_filename = name;
	if (!strcmp(name+len-4, ".pcx"))
	{
		static char png_name[MAX_QPATH];
		memcpy (png_name, name, len+1);
		if (load_tga_pics)
		{
			png_name[len-3] = 't';
			png_name[len-2] = 'g';
			png_name[len-1] = 'a';
			current_texture_filename = png_name;
			LoadTGA (png_name, &pic, &width, &height);
		}
		if (!pic)
		{
			if (load_png_pics)
			{
				png_name[len-3] = 'p';
				png_name[len-2] = 'n';
				png_name[len-1] = 'g';
				LoadPNG (png_name, &pic, &width, &height);
			}
			if (!pic)
			{
				if (load_jpg_pics)
				{
					png_name[len-3] = 'j';
					png_name[len-2] = 'p';
					LoadJPG (png_name, &pic, &width, &height);
				}
				if (!pic)
				{
					current_texture_filename = name;
					LoadPCX (name, &pic, &palette, &width, &height);
					if (!pic)
						return NULL;
					bpp = 8;
				}
				else
				{
					bpp = 32;
				}
			}
			else
			{
				bpp = 32;
			}
		}
		else
		{
			bpp = 32;
		}
		image = GL_LoadPic (name, pic, width, height, type, bpp);
	}
	else if (!strcmp(name+len-4, ".png"))
	{
		LoadPNG (name, &pic, &width, &height);
		if (!pic)
			return NULL; // ri.Sys_Error (ERR_DROP, "GL_FindImage: can't load %s", name);
		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else if (!strcmp(name+len-4, ".wal"))
	{
		image = GL_LoadWal (name);
	}
	else if (!strcmp(name+len-4, ".jpg"))
	{
		LoadJPG (name, &pic, &width, &height);
		if (!pic)
			return NULL;
		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else if (!strcmp(name+len-4, ".tga"))
	{
		LoadTGA (name, &pic, &width, &height);
		if (!pic)
			return NULL;
		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else
		return NULL;	//	ri.Sys_Error (ERR_DROP, "GL_FindImage: bad extension on: %s", name);

	//newitem = rbsearch (name, rb);
	//*newitem = image;

	strncpy (image->basename, basename, sizeof(image->basename)-1);

	image->hash_next = images_hash[hash];
	images_hash[hash] = image;

	if (pic)
		free(pic);

	if (palette)
		free(palette);

	return image;
}



/*
===============
R_RegisterSkin
===============
*/
struct image_s * EXPORT R_RegisterSkin (char *name)
{
	return GL_FindImage (name, name, it_skin);
}

/*
===============
GL_ImageList_f
===============
*/
void	GL_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;
	int		spr = 0;
	int		ski = 0;
	int		wal = 0;
	int		pic = 0;
	int		mis = 0;
	/*const char *palstrings[2] =
	{
		"RGB",
		"PAL"
	};*/

	ri.Con_Printf (PRINT_ALL, "------------------\n");
	texels = 0;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (!image->texnum)
			continue;
		texels += image->upload_width*image->upload_height;
		switch (image->type)
		{
		case it_skin:
			ski++;
			ri.Con_Printf (PRINT_ALL, "M");
			break;
		case it_sprite:
			spr++;
			ri.Con_Printf (PRINT_ALL, "S");
			break;
		case it_wall:
			wal++;
			ri.Con_Printf (PRINT_ALL, "W");
			break;
		case it_pic:
			pic++;
			ri.Con_Printf (PRINT_ALL, "P");
			break;
		default:
			mis++;
			ri.Con_Printf (PRINT_ALL, " ");
			break;
		}

		ri.Con_Printf (PRINT_ALL,  " %3i x %3i: %s (%d bytes)\n",
			image->upload_width, image->upload_height, image->name, image->upload_width * image->upload_height * sizeof(int));
	}

	ri.Con_Printf (PRINT_ALL, "%d skins (M), %d sprites (S), %d world textures (W), %d pics (P), %d misc.\n", ski, spr, wal, pic, mis);

	/*
	ri.Con_Printf (PRINT_ALL, "ImageCache: %d level 1 hits, %d level 2 hits, %d cache misses. Efficiency = %.2f%%\n",
		l1cachehits,
		l2cachehits,
		cachemisses,
		((float)(l1cachehits+(l2cachehits/4)) / (float)(l1cachehits+l2cachehits+cachemisses) * 100.0));
	*/

	ri.Con_Printf (PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}

/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void GL_FreeUnusedImages (void)
{
	int		count;
	int		i;
	int		hash;
	image_t	*image;
	image_t	*hashstart;
	image_t	**prev;

	if (scaled_buffer)
	{
		free (scaled_buffer);
		scaled_buffer = NULL;
	}

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	count = 0;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence

		if (!image->registration_sequence)
			continue;		// free image_t slot

		if (image->type == it_pic)
			continue;		// don't free pics

		hash = hashify (image->basename) % IMAGES_HASH_SIZE;
		/*prev = NULL;
		for(hashstart = images_hash[hash]; hashstart; hashstart = hashstart->hash_next)
		{
			if (hashstart == image)
			{
				if (prev)
					prev->hash_next = hashstart->hash_next;
				else
					images_hash[hash] = images_hash[hash]->hash_next;
				break;
			}
			prev = hashstart;
		}*/
		
		prev = &images_hash[hash];
		for (;;)
		{
			hashstart = *prev;
			if (!hashstart)
				break;
			if (hashstart == image)
			{
				*prev = hashstart->hash_next;
				break;
			}
			prev = &hashstart->hash_next;
		}
		// free it

		count++;
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}

	ri.Con_Printf (PRINT_DEVELOPER, "GL_FreeUnusedImages: freed %d images\n", count);
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette (void)
{
	int		i;
	int		r, g, b;
	unsigned	v;
	byte	*pic, *pal;
	int		width, height;

	// get the palette

	LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (i=0 ; i<256 ; i++)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	free (pic);
	free (pal);

	return 0;
}


/*
===============
GL_InitImages
===============
*/
void	GL_InitImages (void)
{
	int		i, j;
	float	g = vid_gamma->value;

	registration_sequence = 1;

#ifdef RB_IMAGE_CACHE
	rb = rbinit (strcmp);
#endif

	// init intensity conversions
	intensity = ri.Cvar_Get ("intensity", "2", CVAR_ARCHIVE);

	if ( intensity->value <= 1 )
		ri.Cvar_Set( "intensity", "1" );

	if (FLOAT_NE_ZERO(gl_overbrights->value))
		g = 1.0;

	gl_state.inverse_intensity = 1 / intensity->value;

	Draw_GetPalette ();

	if ( qglColorTableEXT )
	{
		ri.FS_LoadFile( "pics/16to8.dat", (void *)&gl_state.d_16to8table );
		if ( !gl_state.d_16to8table )
			ri.Sys_Error( ERR_FATAL, "Couldn't load pics/16to8.pcx");
	}

	if ( gl_config.renderer & ( GL_RENDERER_VOODOO | GL_RENDERER_VOODOO2 ) )
	{
		g = 1.0F;
	}

	for ( i = 0; i < 256; i++ )
	{
		if ( g == 1 )
		{
			gammatable[i] = i;
		}
		else
		{
			float inf;

			inf = 255 * (float)pow ( (i+0.5f)/255.5f , g ) + 0.5f;
			if (FLOAT_LT_ZERO(inf))
				inf = 0;
			if (inf > 255)
				inf = 255;
			gammatable[i] = (byte)Q_ftol(inf);
		}
	}


	for (i=0 ; i<256 ; i++)
	{
		j = (int)((float)i * intensity->value);
		if (j > 255)
			j = 255;
		intensitytable[i] = j;
	}

	for (i=0 ; i<256 ; i++)
	{
		gammaintensitytable[i] = gammatable[intensitytable[i]];
	}
}

/*
===============
GL_ShutdownImages
===============
*/
void	GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;

#ifdef RB_IMAGE_CACHE
	DestroyImageCache ();
#endif

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot
		// free it
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}
}

