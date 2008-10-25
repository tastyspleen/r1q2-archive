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
// r_misc.c

#ifdef WIN32
#include <process.h>
#endif

#include "gl_local.h"
#include <jpeglib.h>
#include <png.h>

/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8][4];

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}

	r_particletexture = GL_FindImage ("particle.png", "particle", it_sprite);
	
	if (!r_particletexture)
		r_particletexture = GL_FindImage ("particle.tga", "particle", it_sprite);

	if (!r_particletexture)
		r_particletexture = GL_LoadPic ("***particle***", (byte *)data, 8, 8, it_sprite, 32);

	//
	// also use this for bad textures, but without alpha
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0; // dottexture[x&3][y&3]*255;
			data[y][x][2] = 0; //dottexture[x&3][y&3]*255;
			data[y][x][3] = 255;
		}
	}
	r_notexture = GL_LoadPic ("***r_notexture***", (byte *)data, 8, 8, it_wall, 32);
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void	FS_CreatePath (char *path)
{
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
			Sys_Mkdir (path);
			*ofs = '/';
		}
	}
}

#define USE_THREADS 1

#ifdef WIN32
extern void png_default_flush(png_structp png_ptr);
extern void png_default_write_data(png_structp png_ptr, png_bytep data, png_size_t length);
unsigned int __stdcall png_write_thread (byte *buffer)
{
	char		picname[MAX_OSPATH]; 
	char		checkname[MAX_OSPATH];
	int			i;
	FILE		*f;
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned	k;
	png_bytepp	row_pointers;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/", ri.FS_Gamedir());
	FS_CreatePath (checkname);

	for (i = 0; i < 999; i++) {
		sprintf (picname, "%s/scrnshot/quake%.3d.png", ri.FS_Gamedir(), i);
		f = fopen (picname, "rb");
		if (!f)
			break;
		fclose (f);
	}

	f = fopen (picname, "wb");
	if (!f)
	{
		ri.Con_Printf (PRINT_ALL, "Couldn't open %s for writing.\n", picname);
#ifdef USE_THREADS
		ExitThread (1);
#else
		return 1;
#endif
	}

    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) {
		ri.Con_Printf (PRINT_ALL, "libpng error\n", picname);
#ifdef USE_THREADS
		ExitThread (1);
#else
		return 1;
#endif
	}

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
       png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
       ri.Con_Printf (PRINT_ALL, "libpng error\n", picname);
#ifdef USE_THREADS
		ExitThread (1);
#else
		return 1;
#endif
    }

	png_init_io(png_ptr, f);

	png_set_IHDR(png_ptr, info_ptr, vid.width, vid.height, 8, PNG_COLOR_TYPE_RGB,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_set_compression_level(png_ptr, Z_DEFAULT_COMPRESSION);
	png_set_compression_mem_level(png_ptr, 9);
	png_set_compression_buffer_size(png_ptr, vid.width * vid.height * 3);

	png_write_info(png_ptr, info_ptr);

	row_pointers = malloc(vid.height * sizeof(png_bytep));
	if (!row_pointers)
		ri.Sys_Error (ERR_FATAL, "png_write_thread: out of memory");

	for (k = 0; k < vid.height; k++)
		row_pointers[k] = buffer + (vid.height-1-k)*3*vid.width;

	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose (f);

	free (buffer);
	free (row_pointers);

	ri.Con_Printf (PRINT_ALL, "Finished, wrote %s\n", picname);
#ifdef USE_THREADS
	ExitThread (0);
#endif

	return 0;
}
#endif

void GL_ScreenShot_JPG (byte *buffer)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW s[1];
	FILE *f;
	char picname[80], checkname[MAX_OSPATH];
	int i, offset, w3;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/", ri.FS_Gamedir());
	FS_CreatePath (checkname);

	for (i = 0; i < 999; i++) {
		sprintf (picname, "%s/scrnshot/quake%.3d.jpg", ri.FS_Gamedir(), i);
		f = fopen (picname, "rb");
		if (!f)
			break;
		fclose (f);
	}

	f = fopen (picname, "wb");
	if (!f)
	{
		ri.Con_Printf (PRINT_ALL, "Couldn't open %s for writing.\n", picname);
		return;
	}

	// Initialise the JPEG compression object
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, f);

	// Setup JPEG Parameters
	cinfo.image_width = vid.width;
	cinfo.image_height = vid.height;
	cinfo.in_color_space = JCS_RGB;
	cinfo.input_components = 3;

	jpeg_set_defaults(&cinfo);

	// Niceass: 85 is the quality. 0-100
	jpeg_set_quality(&cinfo, Q_ftol(gl_jpg_quality->value), TRUE);

	// Start Compression
	jpeg_start_compress(&cinfo, true);

	// Feed scanline data
	w3 = cinfo.image_width * 3;
	offset = w3 * cinfo.image_height - w3;

	while (cinfo.next_scanline < cinfo.image_height)
	{
		s[0] = &buffer[offset - cinfo.next_scanline * w3];
		jpeg_write_scanlines(&cinfo, s, 1);
	}

	// Finish Compression
	jpeg_finish_compress(&cinfo);

	jpeg_destroy_compress(&cinfo);

	fclose(f);
	free(buffer);

	ri.Con_Printf (PRINT_ALL, "Wrote %s\n", picname);
}

/* 
================== 
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	byte		*buffer;
#ifdef WIN32
	DWORD tID;
#endif

	buffer = malloc(vid.width*vid.height*3);

	qglReadPixels (0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer ); 
#ifdef WIN32
	if (!strcmp (ri.Cmd_Argv(1), "jpg"))
	{
		GL_ScreenShot_JPG (buffer);
	}
	else
	{
#ifdef USE_THREADS
		//_beginthreadex (NULL, 0, (unsigned int (__stdcall *)(void *))png_write_thread, (void *)buffer, 0, &tID);
		CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)png_write_thread, (LPVOID)buffer, 0, &tID);
		ri.Con_Printf (PRINT_ALL, "Taking PNG screenshot...\n");
#else
		png_write_thread (buffer);
#endif
	}
#else
	GL_ScreenShot_JPG (buffer);
#endif
} 

#ifdef _DEBUG
void GL_CheckForError (void)
{
	int error;

	error = qglGetError ();

	if (error != GL_NO_ERROR)
		ri.Sys_Error (ERR_FATAL, "qglGetError: %d", error);
}
#else
void GL_CheckForError (void)
{
}
#endif

/*
** GL_Strings_f
*/
void GL_Strings_f( void )
{
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string );
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string );
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string );
	ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

#ifdef R1GL_RELEASE
void GL_Version_f (void)
{
	char buffer[1024];
	snprintf (buffer, sizeof(buffer)-1, "echo Version: "REF_VERSION"\ncmd say \"I'm using "REF_VERSION" (%s/%s) %s | http://r1gl.r1.cx/\"", gl_config.vendor_string, gl_config.renderer_string,
		gl_config.wglPFD ? va("%dc/%dd/%da/%ds [WGL]", (int)gl_colorbits->value, (int)gl_depthbits->value, (int)gl_alphabits->value, (int)gl_stencilbits->value)
		: va("%dc GL", gl_config.bitDepth));
	ri.Cmd_ExecuteText (EXEC_APPEND, buffer);
}
#endif

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	qglClearColor (1.0f, 0.0f, 0.5f, 0.5f);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666f);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);

	qglColor4fv(colorWhite);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel (GL_FLAT);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv( GL_REPLACE );

	if ( qglPointParameterfEXT && FLOAT_NE_ZERO(gl_ext_pointparameters->value))
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable( GL_POINT_SMOOTH );
		qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
	}

	gl_swapinterval->modified = true;
	GL_UpdateSwapInterval();
}

void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified )
	{
		gl_swapinterval->modified = false;

#ifdef STEREO_SUPPORT
		if ( !gl_state.stereo_enabled ) 
#endif
		{
#ifdef _WIN32
			if ( qwglSwapIntervalEXT )
			{
				qwglSwapIntervalEXT( Q_ftol(gl_swapinterval->value) );
			}
#endif
		}
	}
}
