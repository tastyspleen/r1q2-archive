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
// console.c

#include "client.h"
#include <time.h>

console_t	con;

cvar_t		*con_notifytime;

extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;
		

void DrawString (int x, int y, const char *s)
{
	//r1: don't draw if obscured
	if (viddef.height * scr_conlines > y)
		return;

	while (*s)
	{
		re.DrawChar (x, y, *s);
		x+=8;
		s++;
	}
}

void DrawAltString (int x, int y, const char *s)
{
	//r1: don't draw if obscured
	if (viddef.height * scr_conlines > y)
		return;

	while (*s)
	{
		re.DrawChar (x, y, *s ^ 0x80);
		x+=8;
		s++;
	}
}


void Key_ClearTyping (void)
{
	key_lines[edit_line][1] = 0;	// clear any typing
	key_linepos = 1;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque ();	// get rid of loading plaque

	//r1: fucking annoying.
	/*if (cl.attractloop)
	{
		cl.attractloop = false;
		Cmd_ExecuteString ("disconnect\n");
		return;
	}*/

	if (cls.state == ca_disconnected)
	{	// start the demo loop again

		//r1: fucking annoying.
		//Cbuf_AddText ("d1\n");
		cls.key_dest = key_console;
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	if (cls.key_dest == key_console)
	{
		M_ForceMenuOff ();
		//if (chat_bufferlen)
		//	cls.key_dest = key_message;
		//Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff ();
		cls.key_dest = key_console;	

		/*if (Cvar_VariableValue ("maxclients") == 1 
			&& Com_ServerState ())
			Cvar_Set ("paused", "1");*/
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			M_ForceMenuOff ();
			cls.key_dest = key_game;
		}
	}
	else
		cls.key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
static void Con_Clear_f (void)
{
	memset (con.text, ' ', sizeof(con.text));
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f (void)
{
	int		l, x;
	char	*line;
	FILE	*f;
	char	buffer[1024];
	char	name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n", LOG_CLIENT);
		return;
	}

	//r1: consolidate condumps to their own dir
	Com_sprintf (name, sizeof(name), "%s/condumps/%s.txt", FS_Gamedir(), Cmd_Argv(1));

	if (strstr (Cmd_Argv(1), "..") || strchr (Cmd_Argv(1), '/') || strchr (Cmd_Argv(1), '\\') )
	{
		Com_Printf ("Illegal filename.\n", LOG_CLIENT);
		return;
	}

	FS_CreatePath (name);
	f = fopen (name, "w");
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n", LOG_CLIENT);
		return;
	}

	//r1: save some basic infos
	if (cls.state == ca_active) {
		time_t timet;
		time(&timet);
		fprintf (f, "%s on server %s, map %s.\n-----------------\n", ctime (&timet), cls.servername, cl.configstrings[CS_MODELS+1] + 5);
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if (line[x] != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		strncpy (buffer, line, con.linewidth);
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x=0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	Com_Printf ("Dumped console text to %s.\n", LOG_CLIENT, name);

	fclose (f);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	/*int		i;
	
	for (i=0 ; i<NUM_CON_TIMES ; i++)
		con.times[i] = 0;*/
	memset (&con.times, 0, sizeof (con.times));
}

						
/*
================
Con_MessageMode_f
================
*/
static void Con_MessageMode_f (void)
{
	if (cls.state != ca_active)
		return;

	chat_mode = CHAT_MODE_PUBLIC;

	if (!chat_bufferlen && Cmd_Argc() > 1)
	{
		Q_strncpy (chat_buffer[chat_curbuffer], Cmd_Args(), sizeof(chat_buffer[chat_curbuffer])-2);
		if (chat_buffer[chat_curbuffer][0])
		{
			strcat (chat_buffer[chat_curbuffer], " ");
			chat_bufferlen = (int)strlen(chat_buffer[chat_curbuffer]);
			chat_cursorpos = chat_bufferlen;
		}
	}

	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
static void Con_MessageMode2_f (void)
{
	if (cls.state != ca_active)
		return;

	chat_mode = CHAT_MODE_TEAM;

	if (!chat_bufferlen && Cmd_Argc() > 1)
	{
		Q_strncpy (chat_buffer[chat_curbuffer], Cmd_Args(), sizeof(chat_buffer[chat_curbuffer])-2);
		if (chat_buffer[chat_curbuffer][0])
		{
			strcat (chat_buffer[chat_curbuffer], " ");
			chat_bufferlen = (int)strlen(chat_buffer[chat_curbuffer]);
			chat_cursorpos = chat_bufferlen;
		}
	}

	cls.key_dest = key_message;
}

static void Con_MessageModex_f (void)
{
	if (cls.state != ca_active)
		return;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("Usage: messagemodex prompt [default]\n", LOG_GENERAL);
		return;
	}

	chat_mode = CHAT_MODE_CUSTOM;

	strncpy (chat_custom_prompt, Cmd_Argv(1), sizeof(chat_custom_prompt)-3);
	strcpy (chat_custom_cmd, chat_custom_prompt);
	strcat (chat_custom_prompt, ": ");

	if (!chat_bufferlen && Cmd_Argc() > 2)
	{
		Q_strncpy (chat_buffer[chat_curbuffer], Cmd_Args2(2), sizeof(chat_buffer[chat_curbuffer])-2);
		if (chat_buffer[chat_curbuffer][0])
		{
			strcat (chat_buffer[chat_curbuffer], " ");
			chat_bufferlen = (int)strlen(chat_buffer[chat_curbuffer]);
			chat_cursorpos = chat_bufferlen;
		}
	}

	cls.key_dest = key_message;
}

static void Con_Resize (int width)
{
	char	tbuf[CON_TEXTSIZE];
	int		i, j, oldwidth, oldtotallines, numlines, numchars;

	oldwidth = con.linewidth;
	con.linewidth = width;
	oldtotallines = con.totallines;
	con.totallines = CON_TEXTSIZE / con.linewidth;
	numlines = oldtotallines;

	if (con.totallines < numlines)
		numlines = con.totallines;

	numchars = oldwidth;

	if (con.linewidth < numchars)
		numchars = con.linewidth;

	memcpy (tbuf, con.text, CON_TEXTSIZE);
	memset (con.text, ' ', CON_TEXTSIZE);

	for (i=0 ; i<numlines ; i++)
	{
		for (j=0 ; j<numchars ; j++)
		{
			con.text[(con.totallines - 1 - i) * con.linewidth + j] =
					tbuf[((con.current - i + oldtotallines) %
							oldtotallines) * oldwidth + j];
		}
	}
}
/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		width;

	width = (viddef.width >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con.linewidth = width;
		con.totallines = sizeof(con.text) / con.linewidth;
		memset (con.text, ' ', sizeof(con.text));
	}
	else
	{
		Con_Resize (width);
		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}


/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con.linewidth = -1;

	Con_CheckResize ();
	
	Com_Printf ("Console initialized.\n", LOG_CLIENT);

	cls.key_dest = key_console;

//
// register our commands
//
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("messagemodex", Con_MessageModex_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed (void)
{
	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	memset (&con.text[(con.current%con.totallines)*con.linewidth]
	, ' ', con.linewidth);
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print (const char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	if (!con.initialized)
		return;

	if (txt[0] == 1 || txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
	// count word length
		for (l=0 ; l< con.linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

	// word wrap
		if (l != con.linewidth && (con.x + l > con.linewidth) )
			con.x = 0;

		txt++;

		if (cr)
		{
			con.current--;
			cr = false;
		}

		
		if (!con.x)
		{
			Con_Linefeed ();
		// mark time for transparent overlay
			if (con.current >= 0)
				con.times[con.current % NUM_CON_TIMES] = cls.realtime;
		}

		switch (c)
		{
		case '\n':
			con.x = 0;
			break;

		case '\r':
			con.x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = c | mask | con.ormask;
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;
			break;
		}
		
	}
}


/*
==============
Con_CenteredPrint
==============
*/
/*void Con_CenteredPrint (char *text)
{
	int		l;
	char	buffer[1024];

	l = strlen(text);
	l = (con.linewidth-l)/2;
	if (l < 0)
		l = 0;
	memset (buffer, ' ', l);
	strcpy (buffer+l, text);
	strcat (buffer, "\n");
	Con_Print (buffer);
}*/

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
static void Con_DrawInput (void)
{
	int		linepos;
	int		length;
	int		i;
	char	*text;

	if (cls.key_dest == key_menu)
		return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (always draw if not active)

	text = key_lines[edit_line];
	
// add the cursor frame
	//text[key_linepos] = 10+((int)(cls.realtime>>8)&1);
	
// fill out remainder with spaces
	//for (i=key_linepos+1 ; i< con.linewidth ; i++)
	//	text[i] = ' ';
		
//	prestep if horizontally scrolling
	if (key_linepos + 1  >= con.linewidth)
	{
		linepos = con.linewidth;
		text += 1 + key_linepos - con.linewidth;
	}
	else
	{
		linepos = key_linepos + 1;
	}
		
// draw it
	//y = con.vislines-16;

	length = (int)strlen (text);

	for (i=0 ; i<length; i++)
		re.DrawChar ( (i+1)<<3, con.vislines - 22, text[i]);

	if (((int)(cls.realtime>>8)&1))
		re.DrawChar ( (linepos)<<3, con.vislines - 21, '_');

// remove cursor
	//key_lines[edit_line][key_linepos] = 0;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	const char	*text;
	int		i;
	int		time;
	char	*s;

	v = 0;
	for (i= con.current-NUM_CON_TIMES+1 ; i<=con.current ; i++)
	{
		if (i < 0)
			continue;
		time = con.times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = cls.realtime - time;
		if (time > con_notifytime->intvalue*1000)
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;
		
		for (x = 0 ; x < con.linewidth ; x++)
			re.DrawChar ( (x+1)<<3, v, text[x]);

		v += 8;
	}


	if (cls.key_dest == key_message)
	{
		int		cursorpos;
		int		maxwidth;
		int		skip = 0;

		switch (chat_mode)
		{
			case CHAT_MODE_PUBLIC:
				DrawString (8, v, "say:");
				skip = 5;
				break;
			case CHAT_MODE_TEAM:
				DrawString (8, v, "say_team:");
				skip = 11;
				break;
			case CHAT_MODE_CUSTOM:
				DrawString (8, v, chat_custom_prompt);
				skip = (int)strlen (chat_custom_prompt)+1;
				break;
		}

		s = chat_buffer[chat_curbuffer];

		maxwidth =  (viddef.width>>3);

		if (chat_cursorpos > maxwidth-(skip+1))
		{
			s += chat_cursorpos - (maxwidth-(skip+1));
			cursorpos = maxwidth-1;
		}
		else
		{
			cursorpos = chat_cursorpos + skip;
		}

		x = 0;
		while(s[x])
		{
			re.DrawChar ( (x+skip)<<3, v, s[x]);
			x++;
		}

		if (((cls.realtime>>8)&1))
			re.DrawChar ( (cursorpos)<<3, v+1, '_');
		v += 8;
	}
	
	if (v)
	{
		SCR_AddDirtyPoint (0,0);
		SCR_AddDirtyPoint (viddef.width-1, v);
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (float frac)
{
	int				i, j, x, y, n, len, offset;
	int				rows;
	char			*text;
	int				row;
	int				lines;
	char			version[24];
	char			dlbar[1024];

	time_t			t;
	struct tm		*today;

	lines = (int)(viddef.height * frac);

	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

// draw the background
	re.DrawStretchPic (0, lines - viddef.height, viddef.width, viddef.height, "conback");
	SCR_AddDirtyPoint (0,0);
	SCR_AddDirtyPoint (viddef.width-1,lines-1);

	len = (int)strlen(key_lines[edit_line]);

	i = Com_sprintf (version, sizeof(version), PRODUCTNAMELOWER " " VERSION);

	if (len >= (viddef.width * 0.125f) - (i+2))
		offset = 20;
	else
		offset = 0;

	for (x=i-1; x>=0 ; x--)
		re.DrawChar (viddef.width-2-(i*8)+x*8, lines-12-offset, 128 + version[x] );

	t = time (NULL);
	today = localtime(&t);

	i = (int)strftime (version, sizeof(version), "%H:%M:%S", today);
	for (x=0 ; x<i ; x++)
		re.DrawChar (viddef.width-66+x*8, lines-22-offset, 128 + version[x] );

// draw the text
	con.vislines = lines;
	
#if 0
	rows = (lines-8)>>3;		// rows of text to draw

	y = lines - 24;
#else
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;
#endif

// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x=0 ; x<con.linewidth ; x+=4)
			re.DrawChar ( (x+1)<<3, y, '^');
	
		y -= 8;
		rows--;
	}
	
	row = con.display;
	for (i=0 ; i<rows ; i++, y-=8, row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;		// past scrollback wrap point
			
		text = con.text + (row % con.totallines)*con.linewidth;

		for (x=0 ; x<con.linewidth ; x++)
			re.DrawChar ( (x+1)<<3, y, text[x]);
	}

//ZOID
	// draw the download bar
	// figure out width
	if (cls.downloadname[0] && (cls.download || cls.downloadposition))
	{
		if ((text = strrchr(cls.downloadname, '/')) != NULL)
			text++;
		else
			text = cls.downloadname;

		x = con.linewidth - ((con.linewidth * 7) / 40);
		y = x - (int)strlen(text) - 20;
		i = con.linewidth/3;
		if (strlen(text) > i) {
			y = x - i - 11;
			Q_strncpy(dlbar, text, i);
			strcat(dlbar, "...");
		} else
			strcpy(dlbar, text);
		strcat(dlbar, ": ");
		i = (int)strlen(dlbar);
		dlbar[i++] = '\x80';
		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;
			
		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';

		if (cls.download)
			cls.downloadposition = ftell (cls.download);

		sprintf (dlbar + i, " %02d%% (%.02f KB)", cls.downloadpercent, (float)cls.downloadposition / 1024.0);

		j = (int)strlen(dlbar);

		// draw it
		y = con.vislines-12;
		for (i = 0; i < j; i++)
			re.DrawChar ( (i+1)<<3, y, dlbar[i]);
	}
//ZOID

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}


