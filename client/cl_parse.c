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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

int			serverPacketCount;
int			noFrameFromServerPacket;

void CL_Reconnect_f (void);

//=============================================================================

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	//if (strncmp(fn, "players", 7) == 0)
	//	Com_sprintf (dest, destlen, "%s/%s", BASEDIRNAME, fn);
	//else
	Com_sprintf (dest, destlen, "%s/%s", FS_Gamedir(), fn);
}

void CL_FinishDownload (void)
{
#ifdef _DEBUG
	clientinfo_t *ci;
#endif

	int r;
	char	oldn[MAX_OSPATH];
	char	newn[MAX_OSPATH];

	fclose (cls.download);

	FS_FlushCache();

	// rename the temp file to it's final name
	CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
	CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);

	r = rename (oldn, newn);
	if (r)
		Com_Printf ("failed to rename.\n", LOG_CLIENT);

#ifdef _DEBUG
	if (cls.serverProtocol == PROTOCOL_R1Q2 && (strstr(newn, "players"))) {
		for (r = 0; r < cl.maxclients; r++) {
			ci = &cl.clientinfo[r];
			if (ci->deferred)
				CL_ParseClientinfo (r);
		}
	}
#endif

	cls.failed_download = false;
	cls.downloadpending = false;
	cls.downloadname[0] = 0;
	cls.downloadposition = 0;
	cls.download = NULL;
	cls.downloadpercent = 0;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (const char *filename)
{
	FILE	*fp;
	int		length;
	char	*p;
	char	name[MAX_OSPATH];
	static char lastfilename[MAX_OSPATH] = {0};

	//r1: don't attempt same file many times
	if (!strcmp (filename, lastfilename))
		return true;

	strcpy (lastfilename, filename);

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to check a path with .. (%s)\n", LOG_CLIENT, filename);
		return true;
	}

	if (strchr (filename, ' '))
	{
		Com_Printf ("Refusing to check a path containing spaces (%s)\n", LOG_CLIENT, filename);
		return true;
	}

	if (strchr (filename, ':'))
	{
		Com_Printf ("Refusing to check a path containing a colon (%s)\n", LOG_CLIENT, filename);
		return true;
	}

	if (filename[0] == '/')
	{
		Com_Printf ("Refusing to check a path starting with / (%s)\n", LOG_CLIENT, filename);
		return true;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	
		// it exists, no need to download
		return true;
	}

#ifdef USE_CURL
	if (CL_QueueHTTPDownload (filename))
	{
		//we return true so that the precache check keeps feeding us more files.
		//since we have multiple HTTP connections we want to minimize latency
		//and be constantly sending requests, not one at a time.
		return true;
	}
	else
#endif
	{
		strcpy (cls.downloadname, filename);

		//r1: fix \ to /
		p = cls.downloadname;
		while ((p = strchr(p, '\\')))
			p[0] = '/';

		length = (int)strlen(cls.downloadname);

		//normalize path
		p = cls.downloadname;
		while ((p = strstr (p, "./")))
		{
			memmove (p, p+2, length - (p - cls.downloadname) - 1);
			length -= 2;
		}

		//r1: verify we are giving the server a legal path
		if (cls.downloadname[length-1] == '/')
		{
			Com_Printf ("Refusing to download bad path (%s)\n", LOG_CLIENT, filename);
			return true;
		}

		// download to a temp name, and only rename
		// to the real name when done, so if interrupted
		// a runt file wont be left
		COM_StripExtension (cls.downloadname, cls.downloadtempname);
		strcat (cls.downloadtempname, ".tmp");

	//ZOID
		// check to see if we already have a tmp for this file, if so, try to resume
		// open the file if not opened yet
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

	//	FS_CreatePath (name);

		fp = fopen (name, "r+b");
		if (fp)
		{
			// it exists
			int len;
			
			fseek(fp, 0, SEEK_END);
			len = ftell(fp);

			cls.download = fp;

			// give the server an offset to start the download
			Com_Printf ("Resuming %s\n", LOG_CLIENT, cls.downloadname);

			MSG_WriteByte (clc_stringcmd);
			if (cls.serverProtocol == PROTOCOL_R1Q2)
				MSG_WriteString (va("download \"%s\" %i udp-zlib", cls.downloadname, len));
			else
				MSG_WriteString (va("download \"%s\" %i", cls.downloadname, len));
		}
		else
		{
			Com_Printf ("Downloading %s\n", LOG_CLIENT, cls.downloadname);

			MSG_WriteByte (clc_stringcmd);
			if (cls.serverProtocol == PROTOCOL_R1Q2)
				MSG_WriteString (va("download \"%s\" 0 udp-zlib", cls.downloadname));
			else
				MSG_WriteString (va("download \"%s\"", cls.downloadname));
		}

		MSG_EndWriting (&cls.netchan.message);

		send_packet_now = true;
		cls.downloadpending = true;

		return false;
	}
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void CL_Download_f (void)
{
	//char	name[MAX_OSPATH];
	//FILE	*fp;
//	char	*p;
	char	*filename;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n", LOG_CLIENT);
		return;
	}

	if (cls.state < ca_connected)
	{
		Com_Printf ("Not connected.\n", LOG_CLIENT);
		return;
	}

	//Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));
	filename = Cmd_Argv(1);

	if (FS_LoadFile (filename, NULL) != -1)
	{	
		// it exists, no need to download
		Com_Printf("File already exists.\n", LOG_CLIENT);
		return;
	}

	CL_CheckOrDownloadFile (filename);

	/*if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with .. (%s)\n", LOG_CLIENT, filename);
		return;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		Com_Printf("File already exists.\n", LOG_CLIENT);
		return;
	}

	strncpy (cls.downloadname, filename, sizeof(cls.downloadname)-1);


	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

	fp = fopen (name, "r+b");
	if (fp) { // it exists
		int len;
		
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", LOG_CLIENT, cls.downloadname);
		MSG_WriteByte (clc_stringcmd);
		if (cls.serverProtocol == PROTOCOL_R1Q2) {
			MSG_WriteString (va("download \"%s\" %i udp-zlib", cls.downloadname, len));
		} else {
			MSG_WriteString (va("download \"%s\" %i", cls.downloadname, len));
		}
	} else {
		Com_Printf ("Downloading %s\n", LOG_CLIENT, cls.downloadname);
	
		MSG_WriteByte (clc_stringcmd);
		if (cls.serverProtocol == PROTOCOL_R1Q2) {
			MSG_WriteString (va("download \"%s\" 0 udp-zlib", cls.downloadname));
		} else {
			MSG_WriteString (va("download \"%s\" 0", cls.downloadname));
		}
	}
	MSG_EndWriting (&cls.netchan.message);

	send_packet_now = true;*/
}

void CL_Passive_f (void)
{
	if (cls.state != ca_disconnected) {
		Com_Printf ("Passive mode can only be modified when you are disconnected.\n", LOG_CLIENT);
	} else {
		cls.passivemode = !cls.passivemode;

		if (cls.passivemode) {
			NET_Config (NET_CLIENT);
			Com_Printf ("Listening for passive connections on port %d\n", LOG_CLIENT, Cvar_IntValue ("ip_clientport"));
		} else {
			Com_Printf ("No longer listening for passive connections.\n", LOG_CLIENT);
		}
	}
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i;

	S_BeginRegistration ();
	CL_RegisterTEntSounds ();
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	S_EndRegistration ();
}

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/

void CL_ParseDownload (qboolean dataIsCompressed)
{
	int		size, percent;
	char	name[MAX_OSPATH];

	// read the data
	size = MSG_ReadShort (&net_message);
	percent = MSG_ReadByte (&net_message);

	if (size < 0)
	{
		if (size == -1)
			Com_Printf ("Server does not have this file.\n", LOG_CLIENT);
		else
			Com_Printf ("Bad download data from server.\n", LOG_CLIENT);

		//r1: nuke the temp filename
		cls.downloadtempname[0] = 0;
		cls.downloadname[0] = 0;
		cls.failed_download = true;

		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}

		cls.downloadpending = false;
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		if (!cls.downloadtempname[0])
		{
			Com_Printf ("Received download packet without request. Ignored.\n", LOG_CLIENT);
			net_message.readcount += size;
			return;
		}
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf ("Failed to open %s\n", LOG_CLIENT, cls.downloadtempname);
			cls.downloadpending = false;
			CL_RequestNextDownload ();
			return;
		}
	}

	//r1: downloading something, drop to console to show status bar
	SCR_EndLoadingPlaque();

	//r1: if we're stuck with udp, may as well make best use of the bandwidth...
	if (dataIsCompressed)
	{
#ifndef NO_ZLIB
		uint16		uncompressedLen;
		byte		uncompressed[0xFFFF];

		uncompressedLen = MSG_ReadShort (&net_message);

		if (!uncompressedLen)
			Com_Error (ERR_DROP, "uncompressedLen == 0");

		ZLibDecompress (net_message_buffer + net_message.readcount, size, uncompressed, uncompressedLen, -15);
		fwrite (uncompressed, 1, uncompressedLen, cls.download);
		Com_DPrintf ("svc_zdownload(%s): %d -> %d\n", cls.downloadname, size, uncompressedLen);
#else
		Com_Error (ERR_DROP, "Received a unrequested compressed download");
#endif
	}
	else
	{
		fwrite (net_message_buffer + net_message.readcount, 1, size, cls.download);
	}

	net_message.readcount += size;

	if (percent != 100)
	{
		cls.downloadpercent = percent;

		MSG_WriteByte (clc_stringcmd);
		MSG_Print ("nextdl");
		MSG_EndWriting (&cls.netchan.message);
		send_packet_now = true;
	}
	else
	{
		CL_FinishDownload ();

		// get another file if needed
		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
qboolean CL_ParseServerData (void)
{
	char	*str;
	int		i;
	int		newVersion;
	cvar_t	*gameDirHack;
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	cls.state = ca_connected;

// parse protocol version number
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	if (i != PROTOCOL_ORIGINAL && i != PROTOCOL_R1Q2 && !cl.attractloop)
		Com_Error (ERR_HARD, "Server is using unknown protocol %d.", i);

	// game directory
	str = MSG_ReadString (&net_message);
	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir, fucking christ this is messy!
	if ((str[0] && (!fs_gamedirvar->string || !fs_gamedirvar->string[0] || strcmp(fs_gamedirvar->string, str))) ||
		(!str[0] && (fs_gamedirvar->string || fs_gamedirvar->string[0])))
	{
		if (strcmp(fs_gamedirvar->string, str))
		{
			if (cl.attractloop)
			{
				Cvar_ForceSet ("game", str);
				FS_SetGamedir (str);
			}
			else
			{
				Cvar_Set("game", str);
			}
		}
	}

	Cvar_ForceSet ("$game", str);

	gameDirHack = Cvar_FindVar ("game");
	gameDirHack->flags |= CVAR_NOSET;

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

	if (cls.serverProtocol == PROTOCOL_R1Q2)
	{
		cl.enhancedServer = MSG_ReadByte (&net_message);

		newVersion = MSG_ReadShort (&net_message);
		if (newVersion != MINOR_VERSION_R1Q2)
		{
			if (cl.attractloop)
			{
				if (newVersion < MINOR_VERSION_R1Q2)
					Com_Printf ("This demo was recorded with an earlier version of the R1Q2 protocol. It may not play back properly.\n", LOG_CLIENT);
				else
					Com_Printf ("This demo was recorded with a later version of the R1Q2 protocol. It may not play back properly. Please update your R1Q2 client.\n", LOG_CLIENT);
			}
			else
			{
				if (newVersion > MINOR_VERSION_R1Q2)
					Com_Printf ("Server reports a higher R1Q2 protocol number than your client supports. Some features will be unavailable until you update your R1Q2 client.\n", LOG_CLIENT);
				else
					Com_Printf ("Server reports a lower R1Q2 protocol number. The server admin needs to update their server!\n", LOG_CLIENT);
			}

			//cap if server is above us just to be safe
			if (newVersion > MINOR_VERSION_R1Q2)
				newVersion = MINOR_VERSION_R1Q2;
		}

		if (newVersion >= 1903)
		{
			MSG_ReadByte (&net_message);	//was ad
			cl.strafeHack = MSG_ReadByte (&net_message);
		}
		else
		{
			cl.strafeHack = false;
		}
		
		cls.protocolVersion = newVersion;
	}
	else
	{
		cl.enhancedServer = false;
		cl.strafeHack = false;
		cls.protocolVersion = 0;
	}

	Com_DPrintf ("Serverdata packet received. protocol=%d, servercount=%d, attractloop=%d, clnum=%d, game=%s, map=%s, enhanced=%d\n", cls.serverProtocol, cl.servercount, cl.attractloop, cl.playernum, cl.gamedir, str, cl.enhancedServer);

	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		//SCR_PlayCinematic (str);
		// tell the server to advance to the next map / cinematic
		MSG_WriteByte (clc_stringcmd);
		MSG_Print (va("nextserver %i\n", cl.servercount));
		MSG_EndWriting (&cls.netchan.message);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n", LOG_CLIENT);
		Com_Printf ("\2%s\n", LOG_CLIENT, str);

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}

	//CL_FixCvarCheats();
	return true;
}
/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	uint32			bits;
	int				newnum;


	newnum = CL_ParseEntityBits (&bits);
	es = &cl_entities[newnum].baseline;
	CL_ParseDelta (&null_entity_state, es, newnum, bits);
}

void CL_ParseZPacket (void)
{
#ifndef NO_ZLIB
	byte buff_in[MAX_MSGLEN];
	byte buff_out[0xFFFF];

	sizebuf_t sb, old;

	int16 compressed_len = MSG_ReadShort (&net_message);
	int16 uncompressed_len = MSG_ReadShort (&net_message);
	
	if (uncompressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: uncompressed_len <= 0");

	if (compressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: compressed_len <= 0");

	MSG_ReadData (&net_message, buff_in, compressed_len);

	SZ_Init (&sb, buff_out, uncompressed_len);
	sb.cursize = ZLibDecompress (buff_in, compressed_len, buff_out, uncompressed_len, -15);

	old = net_message;
	net_message = sb;
	CL_ParseServerMessage ();
	net_message = old;

	Com_DPrintf ("Got a ZPacket, %d->%d\n", uncompressed_len + 4, compressed_len);
#else
	Com_Error (ERR_DROP, "Receied a zPacket but no zlib in this binary");
#endif
}


/*
================
CL_LoadClientinfo

================
*/
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	//char		original_model_name[MAX_QPATH];
	//char		original_skin_name[MAX_QPATH];

	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	Q_strncpy(ci->cinfo, s, sizeof(ci->cinfo)-1);

	ci->deferred = false;

	// isolate the player's name
	Q_strncpy(ci->name, s, sizeof(ci->name)-1);

	i = 0;

	t = strchr (s, '\\');
	if (t)
	{
		if (t - s >= sizeof(ci->name)-1)
		{
			i = -1;
		}
		else
		{
			ci->name[t-s] = 0;
			s = t+1;
		}
	}

	//r1ch: check sanity of paths: only allow printable data
	t = s;
	while (*t)
	{
		//if (!isprint (*t))
		if (*t <= 32)
		{
			i = -1;
			break;
		}
		t++;
	}

	if (cl_noskins->intvalue || s[0] == 0 || i == -1)
	{
badskin:
		//strcpy (model_filename, "players/male/tris.md2");
		//strcpy (weapon_filename, "players/male/weapon.md2");
		//strcpy (skin_filename, "players/male/grunt.pcx");
		strcpy (ci->iconname, "/players/male/grunt_i.pcx");
		strcpy (model_name, "male");
		ci->model = re.RegisterModel ("players/male/tris.md2");
		//memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		//ci->weaponmodel[0] = re.RegisterModel (weapon_filename);
		ci->skin = re.RegisterSkin ("players/male/grunt.pcx");
		ci->icon = re.RegisterPic (ci->iconname);
	}
	else
	{
		int		length;
		int		j;

		Q_strncpy (model_name, s, sizeof(model_name)-1);

		t = strchr(model_name, '/');
		if (!t)
			t = strchr(model_name, '\\');

		if (!t)
		{
			memcpy (model_name, "male\0grunt\0\0\0\0\0\0", 16);
			s = "male\0grunt";
		}
		else
		{
			t[0] = 0;
		}

		//strcpy (original_model_name, model_name);

		// isolate the skin name
		Q_strncpy (skin_name, s + strlen(model_name) + 1, sizeof(skin_name)-1);
		//strcpy (original_skin_name, s + strlen(model_name) + 1);

		length = (int)strlen (model_name);
		for (j = 0; j < length; j++)
		{
			if (!isvalidchar(model_name[j]))
			{
				Com_DPrintf ("Bad character '%c' in playermodel '%s'\n", model_name[j], model_name);
				goto badskin;
			}
		}

		length = (int)strlen (skin_name);
		for (j = 0; j < length; j++)
		{
			if (!isvalidchar(skin_name[j]))
			{
				Com_DPrintf ("Bad character '%c' in playerskin '%s'\n", skin_name[j], skin_name);
				goto badskin;
			}
		}

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = re.RegisterModel (model_filename);
		if (!ci->model)
		{
			ci->deferred = true;
			//if (!CL_CheckOrDownloadFile (model_filename))
			//	return;

			strcpy(model_name, "male");
			//Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			strcpy (model_filename, "players/male/tris.md2");
			ci->model = re.RegisterModel (model_filename);
		}

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = re.RegisterSkin (skin_filename);

		if (!ci->skin)
		{
			//Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", original_model_name, original_skin_name);
			ci->deferred = true;
			//CL_CheckOrDownloadFile (skin_filename);
		}

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// change model to male
			strcpy(model_name, "male");
			strcpy (model_filename, "players/male/tris.md2");
			ci->model = re.RegisterModel (model_filename);

			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = re.RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, it means that the male model didn't have
		// it, so default to grunt
		if (!ci->skin) {
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name);
			ci->skin = re.RegisterSkin (skin_filename);
		}

		// icon file
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = re.RegisterPic (ci->iconname);

		if (!ci->icon) {
			//Com_sprintf (ci->iconname, sizeof(ci->iconname), "players/%s/%s_i.pcx", original_model_name, original_skin_name);
			ci->deferred = true;
			//ci->icon = re.RegisterPic ("/players/male/grunt_i.pcx");
		}
	}

	// weapon file
	for (i = 0; i < num_cl_weaponmodels; i++)
	{
		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
		ci->weaponmodel[i] = re.RegisterModel(weapon_filename);

		if (!ci->weaponmodel[i])
		{
			//Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", original_model_name, cl_weaponmodels[i]);
			ci->deferred = true;
		}

		if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0)
		{
			// try male
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
			ci->weaponmodel[i] = re.RegisterModel(weapon_filename);
		}

		if (!cl_vwep->intvalue)
			break; // only one when vwep is off
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}

/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	s = cl.configstrings[player+CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}


/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (void)
{
	size_t	length;
	int		i;
	char	*s;
	char	olds[MAX_QPATH];

	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "CL_ParseConfigString: configstring %d >= MAX_CONFIGSTRINGS", i);
	s = MSG_ReadString(&net_message);

	Q_strncpy (olds, cl.configstrings[i], sizeof(olds)-1);

	//Com_Printf ("cs: %i=%s\n", LOG_GENERAL, i, MakePrintable (s));

	//r1ch: only allow statusbar to overflow
	/*if (i >= CS_STATUSBAR && i < CS_AIRACCEL)
		strncpy (cl.configstrings[i], s, (sizeof(cl.configstrings[i]) * (CS_AIRACCEL - i))-1);
	else
		Q_strncpy (cl.configstrings[i], s, sizeof(cl.configstrings[i])-1);*/

	//r1: overflow may be desired by some mods in stats programs for example. who knows.
	length = strlen(s);

	if (length >= (sizeof(cl.configstrings[0]) * (MAX_CONFIGSTRINGS-i)) - 1)
		Com_Error (ERR_DROP, "CL_ParseConfigString: configstring %d exceeds available space", i);

	//r1: don't allow basic things to overflow
	if (i != CS_NAME && i < CS_GENERAL)
	{
		if (i >= CS_STATUSBAR && i < CS_AIRACCEL)
		{
			strncpy (cl.configstrings[i], s, (sizeof(cl.configstrings[i]) * (CS_AIRACCEL - i))-1);
		}
		else
		{
			if (length >= MAX_QPATH)
				Com_Printf ("WARNING: Configstring %d of length %d exceeds MAX_QPATH.\n", LOG_CLIENT|LOG_WARNING, i, (int)length);
			Q_strncpy (cl.configstrings[i], s, sizeof(cl.configstrings[i])-1);
		}
	}
	else
	{
		strcpy (cl.configstrings[i], s);
	}

	// do something apropriate
	if (i == CS_AIRACCEL)
	{
		pm_airaccelerate = (qboolean)atoi(cl.configstrings[CS_AIRACCEL]);
	}
	else if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
	{
		CL_SetLightstyle (i - CS_LIGHTS);
	}
#ifdef CD_AUDIO
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
	}
#endif
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = re.RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}

		//r1: load map whilst connecting to save a bit of time
		/*if (i == CS_MODELS + 1)
		{
			CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &i);
			if (i && i != atoi(cl.configstrings[CS_MAPCHECKSUM]))
				Com_Error (ERR_DROP, "Local map version differs from server: 0x%.8x != 0x%.8x\n",
					i, atoi(cl.configstrings[CS_MAPCHECKSUM]));
		}*/
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			re.RegisterPic (cl.configstrings[i]);
	}
	else if (i == CS_MAXCLIENTS)
	{
		if (!cl.attractloop)
			cl.maxclients = atoi(cl.configstrings[CS_MAXCLIENTS]);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		//r1: hack to avoid parsing non-skins from mods that overload CS_PLAYERSKINS
		//FIXME: how reliable is CS_MAXCLIENTS?
		i -= CS_PLAYERSKINS;
		if (i < cl.maxclients)
		{
			if (cl.refresh_prepped && strcmp(olds, s))
				CL_ParseClientinfo (i);
		}
		else
		{
			Com_DPrintf ("CL_ParseConfigString: Ignoring out-of-range playerskin %d (%s)\n", i, MakePrintable(s, 0));
		}
	}
}

/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num;
    float 	volume;
    float 	attenuation;  
	int		flags;
	float	ofs;

	flags = MSG_ReadByte (&net_message);
	if (flags == -1)
		Com_Error (ERR_DROP, "CL_ParseStartSoundPacket: End of message while reading flags");

	sound_num = MSG_ReadByte (&net_message);
	if (sound_num == -1)
		Com_Error (ERR_DROP, "CL_ParseStartSoundPacket: End of message while reading sound_num");

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0f;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
	{
		int	attn;
		attn = MSG_ReadByte (&net_message);
		if (attn == -1)
			Com_Error (ERR_DROP, "CL_ParseStartSoundPacket: End of message while reading attenuation");
		attenuation = attn / 64.0f;
	}
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
	{
		int	offset;
		offset = MSG_ReadByte (&net_message);
		if (offset == -1)
			Com_Error (ERR_DROP, "CL_ParseStartSoundPacket: End of message while reading offset");

		ofs = offset / 1000.0f;
	}
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		if (channel == -1)
			Com_Error (ERR_DROP, "CL_ParseStartSoundPacket: End of message while reading channel");

		ent = channel>>3;

		if (ent < 0 || ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (&net_message, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}       

static void CL_ServerFPSChanged (void)
{
	centity_t	*cent;
	int			i;

	cl.gunlerp_start = cl.gunlerp_end = 0;

	for (i = 0; i < MAX_ENTITIES; i++)
	{
		cent = &cl_entities[i];
		cent->lerp_time = 0;
	}
}

static void CL_ParseSetting (void)
{
	uint32	setting, value;

	setting = MSG_ReadLong (&net_message);
	value = MSG_ReadLong (&net_message);

	if (setting >= SVSET_MAX)
		return;

	cl.settings[setting] = value;

	//if FPS changed, reset some internal lerp variables
	if (setting == SVSET_FPS)
		CL_ServerFPSChanged ();
}

static void CL_CheckForIP (const char *s)
{
	unsigned int	ip1, ip2, ip3, ip4;
	unsigned int	port;

	port = 0;

	while (s[0])
	{
		if (sscanf (s, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4) == 4)
		{
			if (ip1 < 256 && ip2 < 256 && ip3 < 256 && ip4 < 256)
			{
				const char *p;
				p = strrchr (s, ':');

				if (p)
				{
					p++;
					port = strtoul (p, NULL, 10);
					if (port <= 1024 || port > 65535)
						break;
				}

				if (port == 0)
					port = PORT_SERVER;

				Com_sprintf (cls.followHost, sizeof(cls.followHost), "%u.%u.%u.%u:%u", ip1, ip2, ip3, ip4, port);
				break;
			}
		}
		s++;
	}
}

static void CL_CheckForURL (const char *s)
{
	char	followURL[1024];
	char	*p;

	p = strstr (s, "http://");
	if (p)
	{
		Q_strncpy (followURL, p, sizeof(followURL)-1);
		StripHighBits (followURL, 1);
		p = strchr (followURL, ' ');
		if (p)
			p[0] = '\0';

		Sys_UpdateURLMenu (followURL);
	}
}

void SHOWNET(const char *s)
{
	if (cl_shownet->intvalue>=2)
		Com_Printf ("%3i:%s\n", LOG_CLIENT, net_message.readcount-1, s);
}

void CL_ParsePrint (void)
{
	int		i;
	char	*s;

	i = MSG_ReadByte (&net_message);
	s = MSG_ReadString (&net_message);

	if (i == PRINT_CHAT)
	{
		if (CL_IgnoreMatch (s))
			return;

		S_StartLocalSound ("misc/talk.wav");
		if (cl_filterchat->intvalue)
		{
			StripHighBits(s, (int)cl_filterchat->intvalue == 2);
			strcat (s, "\n");
		}
		con.ormask = 128;

		CL_CheckForIP (s);
		CL_CheckForURL (s);
		SCR_AddChatMessage (s);

		//r1: change !p_version to !version since p is for proxies
		if ((strstr (s, "!r1q2_version") || strstr (s, "!version")) &&
			(cls.lastSpamTime == 0 || cls.realtime > cls.lastSpamTime + 300000))
			cls.spamTime = cls.realtime + (int)(random() * 1500);

		Com_Printf ("%s", LOG_CLIENT|LOG_CHAT, s);
	}
	else
	{
		int		len;

		Com_Printf ("%s", LOG_CLIENT, s);

		//strip newline for trigger match
		len = strlen(s);
		if (s[len-1] == '\n')
			s[len-1] = '\0';

		Cmd_ExecTrigger (s); //Triggers
	}
	
	con.ormask = 0;
}

/*
=====================
CL_ParseServerMessage
=====================
*/
qboolean CL_ParseServerMessage (void)
{
	int			cmd, extrabits;
	char		*s;
	int			oldReadCount;
	qboolean	gotFrame, ret;

//
// if recording demos, copy the message out
//
	if (cl_shownet->intvalue == 1)
		Com_Printf ("%i ", LOG_CLIENT, net_message.cursize);
	else if (cl_shownet->intvalue >= 2)
		Com_Printf ("------------------\n", LOG_CLIENT);

	serverPacketCount++;
	gotFrame = false;
	ret = true;

//
// parse the message
//
	for (;;)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message (%d>%d)", net_message.readcount, net_message.cursize);
			break;
		}

		oldReadCount = net_message.readcount;

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

#ifdef _DEBUG
		if (cmd == 31)
			Sys_DebugBreak ();
#endif

		//r1: more hacky bit stealing in the name of bandwidth
		extrabits = cmd & 0xE0;
		cmd &= 0x1F;

		if (cl_shownet->intvalue>=2)
		{
			if (cmd >= svc_max_enttypes)
				Com_Printf ("%3i:BAD CMD %i\n", LOG_CLIENT, net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}
	
	// other commands
		switch (cmd)
		{
		case svc_muzzleflash:
			CL_ParseMuzzleFlash ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			strncpy (cl.layout, s, sizeof(cl.layout)-1);
			break;

		case svc_inventory:
			CL_ParseInventory ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_nop:
			break;
			
		case svc_disconnect:
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			Com_Error (ERR_DISCONNECT, "Server disconnected\n");
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n", LOG_CLIENT);
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			cls.downloadname[0] = 0;
			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_print:
			CL_ParsePrint ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			
			break;

		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);

			//ugly, but necessary :(
			if (!cl.attractloop || !strcmp(s, "precache\n"))
				Cbuf_AddText (s);
			else
				Com_DPrintf ("WARNING: Demo tried to execute command '%s', ignored.\n", MakePrintable(s, 0));
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			if (!CL_ParseServerData ())
				return true;
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline ();
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (&net_message));
			CL_WriteDemoMessage (net_message.data + oldReadCount, net_message.readcount - oldReadCount, false);
			break;

		case svc_download:
			CL_ParseDownload (false);
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
#ifdef _DEBUG
			Sys_DebugBreak ();
#endif
			Com_Error (ERR_DROP, "Out of place frame data");
			break;

		case svc_frame:
			//note, frame is written to demo stream in a special way (see cl_ents.c)
			CL_ParseFrame (extrabits);
			gotFrame = true;
			break;

		// ************** r1q2 specific BEGIN ****************
		case svc_zpacket:
			//contents of zpackets are written to demo implicity on decompress
			CL_ParseZPacket();
			break;

		case svc_zdownload:
			CL_ParseDownload(true);
			break;

		case svc_playerupdate:
			gotFrame = true;
			ret = false;
			CL_ParsePlayerUpdate ();
			break;

		case svc_setting:
			CL_ParseSetting ();
			break;
		// ************** r1q2 specific END ******************

		default:
#ifdef _DEBUG
			//Sys_DebugBreak ();
#endif
			if (developer->intvalue)
			{
				Com_Printf ("Unknown command char %d, ignoring!!\n", LOG_CLIENT, cmd);
			}
			else
			{
				/*if (cls.serverProtocol != PROTOCOL_ORIGINAL && cls.realtime - cls.connect_time < 30000)
				{
					Com_Printf ("Unknown command byte %d, assuming protocol mismatch. Reconnecting with protocol 34.\nPlease be sure that you and the server are using the latest build of R1Q2.\n", LOG_CLIENT, cmd);
					CL_Disconnect(false);
					cls.serverProtocol = PROTOCOL_ORIGINAL;
					CL_Reconnect_f ();
					return;
				}*/
				Com_Error (ERR_DROP,"CL_ParseServerMessage: Unknown command byte %d (0x%.2x)", cmd, cmd);
			}
			break;

		}
	}

	if (!gotFrame)
		noFrameFromServerPacket++;
	else
		noFrameFromServerPacket = 0;

	//flush this frame
	CL_WriteDemoMessage (NULL, 0, true);

	return ret;
}
