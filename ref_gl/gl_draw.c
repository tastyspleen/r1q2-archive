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

// draw.c

#include "gl_local.h"

image_t		*draw_chars;

extern	qboolean	scrap_dirty;
void Scrap_Upload (void);

#define	MAX_DRAWCHARS	16384

typedef struct drawchars_s
{
	int		x;
	int		y;
	int		num;
	int		pad;
} drawchars_t;

int			defer_drawing;
int			drawcharsindex;
drawchars_t	drawchars[MAX_DRAWCHARS];

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	draw_chars = GL_FindImage ("pics/conchars.pcx", "pics/conchars.pcx", it_pic);
	if (!draw_chars)
		ri.Sys_Error (ERR_FATAL, "R1GL: Couldn't load conchars.pcx\n\nEither you aren't running Quake 2 from the correct directory or you are missing important files.");
	GL_Bind( draw_chars->texnum );
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static float conchars_texoffset[16] =
{
	0, 0.0625, 0.125, 0.1875, 0.25, 0.3125, 0.375, 0.4375, 0.5, 0.5625, 0.625, 0.6875, 0.75, 0.8125, 0.875, 0.9375
};

static float conchars_texlimits[16] =
{
	0.0625, 0.125, 0.1875, 0.25, 0.3125, 0.375, 0.4375, 0.5, 0.5625, 0.625, 0.6875, 0.75, 0.8125, 0.875, 0.9375, 1
};

void Draw_AddText (void)
{
	int		i, x, y, num;
	int		row, col;
	float	frow, fcol, frowbottom, fcolbottom;

	if (!drawcharsindex)
		return;

	if (draw_chars->has_alpha)
	{
		qglDisable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglEnable(GL_BLEND);
		GL_CheckForError ();

		GL_TexEnv(GL_MODULATE);
	}

	GL_Bind (draw_chars->texnum);
	qglBegin (GL_QUADS);

	for (i = 0; i < drawcharsindex; i++)
	{
		num = drawchars[i].num;
		x = drawchars[i].x;
		y = drawchars[i].y;

		row = num>>4;
		col = num&15;

		frow = conchars_texoffset[row];
		fcol = conchars_texoffset[col];

		frowbottom = conchars_texlimits[row];
		fcolbottom = conchars_texlimits[col];

		qglTexCoord2f (fcol, frow);
		qglVertex2i (x, y);
		qglTexCoord2f (fcolbottom, frow);
		qglVertex2i (x+8, y);
		qglTexCoord2f (fcolbottom, frowbottom);
		qglVertex2i (x+8, y+8);
		qglTexCoord2f (fcol, frowbottom);
		qglVertex2i (x, y+8);
	}

	qglEnd ();
	GL_CheckForError ();

	if (draw_chars->has_alpha)
	{
		GL_TexEnv (GL_REPLACE);
		
		qglEnable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglDisable(GL_BLEND);
		GL_CheckForError ();
	}

	drawcharsindex = 0;
}

/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void EXPORT Draw_Char (int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, frowbottom, fcolbottom;

	num &= 0xFF;

	if ( (num&127) == 32 )
		return;		// space

	//r1: dump all draw chars to a buffer so we can do them all at once later
	if (defer_drawing)
	{
		drawchars[drawcharsindex].x = x;
		drawchars[drawcharsindex].y = y;
		drawchars[drawcharsindex].num = num;

		if (++drawcharsindex == MAX_DRAWCHARS)
			ri.Sys_Error (ERR_FATAL, "drawcharsindex == MAX_DRAWCHARS");

		return;
	}


	//if (y <= -8)
	//	return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = conchars_texoffset[row];
	fcol = conchars_texoffset[col];

	frowbottom = conchars_texlimits[row];
	fcolbottom = conchars_texlimits[col];

	GL_Bind (draw_chars->texnum);

	if (draw_chars->has_alpha)
	{
		qglDisable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglEnable(GL_BLEND);
		GL_CheckForError ();

		GL_TexEnv(GL_MODULATE);
	}

	qglBegin (GL_QUADS);
	qglTexCoord2f (fcol, frow);
	qglVertex2i (x, y);
	qglTexCoord2f (fcolbottom, frow);
	qglVertex2i (x+8, y);
	qglTexCoord2f (fcolbottom, frowbottom);
	qglVertex2i (x+8, y+8);
	qglTexCoord2f (fcol, frowbottom);
	qglVertex2i (x, y+8);
	qglEnd ();
	GL_CheckForError ();

	if (draw_chars->has_alpha)
	{
		GL_TexEnv (GL_REPLACE);
		
		qglEnable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglDisable(GL_BLEND);
		GL_CheckForError ();
	}
}


/*
=============
Draw_FindPic
=============
*/
image_t	* EXPORT Draw_FindPic (char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	fast_strlwr (name);

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);
		gl = GL_FindImage (fullname, name, it_pic);
	}
	else
		gl = GL_FindImage (name+1, name+1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void EXPORT Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_StretchPic
=============
*/
void EXPORT Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Can't find pic: %s\n", pic);
		gl = r_notexture;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
	{
		qglDisable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}

	if (gl->has_alpha)
	{
		qglDisable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglEnable(GL_BLEND);
		GL_CheckForError ();

		GL_TexEnv(GL_MODULATE);
	}

	GL_Bind (gl->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2i (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2i (x+w, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2i (x+w, y+h);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2i (x, y+h);
	qglEnd ();

	GL_CheckForError ();

	if (gl->has_alpha)
	{
		GL_TexEnv (GL_REPLACE);
		
		qglEnable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglDisable(GL_BLEND);
		GL_CheckForError ();
	}

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
	{
		qglEnable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}
}


/*
=============
Draw_Pic
=============
*/
void EXPORT Draw_Pic (int x, int y, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);

	if (!gl)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Can't find pic: %s\n", pic);
		gl = r_notexture;
	}

	if (scrap_dirty)
		Scrap_Upload ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) && !gl->has_alpha)
	{
		qglDisable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}

	if (gl->has_alpha)
	{
		qglDisable(GL_ALPHA_TEST);
		GL_CheckForError ();

		qglEnable(GL_BLEND);
		GL_CheckForError ();

		GL_TexEnv(GL_MODULATE);
	}

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2i (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2i (x+gl->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2i (x+gl->width, y+gl->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2i (x, y+gl->height);
	qglEnd ();

	GL_CheckForError ();

	if (gl->has_alpha)
	{
		GL_TexEnv (GL_REPLACE);
		qglEnable(GL_ALPHA_TEST);
		GL_CheckForError ();
		qglDisable(GL_BLEND);
		GL_CheckForError ();
	}

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !gl->has_alpha)
	{
		qglEnable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void EXPORT Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);

	if (!image)
	{
		ri.Con_Printf (PRINT_DEVELOPER, "Can't find pic: %s\n", pic);
		image = r_notexture;
	}

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
	{
		qglDisable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}

	GL_Bind (image->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x/64.0f, y/64.0f);
	qglVertex2i (x, y);
	qglTexCoord2f ( (x+w)/64.0f, y/64.0f);
	qglVertex2i (x+w, y);
	qglTexCoord2f ( (x+w)/64.0f, (y+h)/64.0f);
	qglVertex2i (x+w, y+h);
	qglTexCoord2f ( x/64.0f, (y+h)/64.0f );
	qglVertex2i (x, y+h);
	qglEnd ();
	GL_CheckForError ();

	if ( ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) )  && !image->has_alpha)
	{
		qglEnable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void EXPORT Draw_Fill (int x, int y, int w, int h, int c)
{
	union
	{
		unsigned	c;
		byte		v[4];
	} color;

	if ( (unsigned)c > 255)
		ri.Sys_Error (ERR_FATAL, "Draw_Fill: bad color");

	qglDisable (GL_TEXTURE_2D);
	GL_CheckForError ();

	color.c = d_8to24table[c];
	qglColor3f (color.v[0]/255.0f,
		color.v[1]/255.0f,
		color.v[2]/255.0f);

	qglBegin (GL_QUADS);

	qglVertex2i (x,y);
	qglVertex2i (x+w, y);
	qglVertex2i (x+w, y+h);
	qglVertex2i (x, y+h);

	qglEnd ();
	GL_CheckForError ();

	qglColor3f (1,1,1);
	GL_CheckForError ();

	qglEnable (GL_TEXTURE_2D);
	GL_CheckForError ();
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void EXPORT Draw_FadeScreen (void)
{
	qglEnable (GL_BLEND);
	GL_CheckForError ();

	qglDisable (GL_TEXTURE_2D);
	GL_CheckForError ();

	qglColor4f (0, 0, 0, 0.8f);
	GL_CheckForError ();

	qglBegin (GL_QUADS);

	qglVertex2i (0,0);
	qglVertex2i (vid.width, 0);
	qglVertex2i (vid.width, vid.height);
	qglVertex2i (0, vid.height);

	qglEnd ();
	GL_CheckForError ();

	qglColor4fv(colorWhite);
	GL_CheckForError ();

	qglEnable (GL_TEXTURE_2D);
	GL_CheckForError ();

	qglDisable (GL_BLEND);
	GL_CheckForError ();
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern unsigned	r_rawpalette[256];

void EXPORT Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	unsigned	image32[256*256];
	unsigned char image8[256*256];
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t;

	GL_Bind (0);

	if (rows<=256)
	{
		hscale = 1;
		trows = rows;
	}
	else
	{
		hscale = rows/256.0f;
		trows = 256;
	}
	t = rows*hscale / 256;

	if ( !qglColorTableEXT )
	{
		unsigned *dest;

		for (i=0 ; i<trows ; i++)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			dest = &image32[i*256];
			fracstep = cols*0x10000/256;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j++)
			{
				dest[j] = r_rawpalette[source[frac>>16]];
				frac += fracstep;
			}
		}

		qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
		GL_CheckForError ();
	}
	else
	{
		unsigned char *dest;

		for (i=0 ; i<trows ; i++)
		{
			row = (int)(i*hscale);
			if (row > rows)
				break;
			source = data + cols*row;
			dest = &image8[i*256];
			fracstep = cols*0x10000/256;
			frac = fracstep >> 1;
			for (j=0 ; j<256 ; j++)
			{
				dest[j] = source[frac>>16];
				frac += fracstep;
			}
		}

		qglTexImage2D( GL_TEXTURE_2D, 
			           0, 
					   GL_COLOR_INDEX8_EXT, 
					   256, 256, 
					   0, 
					   GL_COLOR_INDEX, 
					   GL_UNSIGNED_BYTE, 
					   image8 );
		GL_CheckForError ();
	}
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	GL_CheckForError ();

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GL_CheckForError ();

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
	{
		qglDisable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2i (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2i (x+w, y);
	qglTexCoord2f (1, t);
	qglVertex2i (x+w, y+h);
	qglTexCoord2f (0, t);
	qglVertex2i (x, y+h);
	qglEnd ();
	GL_CheckForError ();

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
	{
		qglEnable (GL_ALPHA_TEST);
		GL_CheckForError ();
	}
}

