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

#ifdef _DEBUG
typedef struct dlqueue_s
{
	struct dlqueue_s	*next;
	struct dlqueue_s	*prev;
	char				filename[MAX_QPATH];
} dlqueue_t;

dlqueue_t downloadqueue;

void CL_AddToDownloadQueue (char *path)
{
	dlqueue_t *dlq = &downloadqueue;

	return;

	if (!Cvar_IntValue ("allow_download"))
		return;

	while (dlq->next) {
		dlq  = dlq->next;

		if (!Q_stricmp (path, dlq->filename))
			return;
	}

	dlq->next = Z_TagMalloc (sizeof(dlqueue_t), TAGMALLOC_CLIENT_DOWNLOAD);	
	dlq->next->prev = dlq;
	dlq = dlq->next;
	dlq->next = NULL;
	strncpy (dlq->filename, path, sizeof(dlq->filename)-1);

	Com_Printf ("DLQ: Added %s\n", LOG_CLIENT, dlq->filename);
}

void CL_RemoveFromDownloadQueue (char *path)
{
	dlqueue_t *dlq = &downloadqueue;

	while (dlq->next) {
		dlq  = dlq->next;

		if (!Q_stricmp (path, dlq->filename)) {
			if (dlq->next)
				dlq->next->prev = dlq->prev;
			dlq->prev->next = dlq->next;

			Z_Free (dlq);
			return;
		}
	}
}

void CL_FlushDownloadQueue (void)
{
	dlqueue_t *old = NULL, *dlq = &downloadqueue;

	while (dlq->next) {
		dlq  = dlq->next;

		if (old)
			Z_Free (old);

		old = dlq;
	}

	if (old)
		Z_Free (old);

	downloadqueue.next = NULL;
}

void CL_RunDownloadQueue (void)
{
	dlqueue_t *old, *dlq = &downloadqueue;

	if (cls.download || cls.downloadpending || cls.state < ca_active)
		return;

	while (dlq->next) {
		dlq  = dlq->next;

		if (CL_CheckOrDownloadFile (dlq->filename)) {
			Com_Printf ("DLQ: Removed %s\n", LOG_CLIENT, dlq->filename);
			dlq->prev->next = dlq->next;
			if (dlq->next)
				dlq->next->prev = dlq->prev;

			old = dlq;
			dlq = dlq->prev;
			Z_Free (old);
		} else {
			Com_Printf ("DLQ: Started %s\n", LOG_CLIENT, dlq->filename);
			return;
		}
	}
}
#endif

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
	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION && (strstr(newn, "players"))) {
		for (r = 0; r < MAX_CLIENTS; r++) {
			ci = &cl.clientinfo[r];
			if (ci->deferred)
				CL_ParseClientinfo (r);
		}
	}
#endif

	cls.downloadpending = false;
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
qboolean	CL_CheckOrDownloadFile (char *filename)
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

	strcpy (cls.downloadname, filename);

	//r1: fix \ to /
	while ((p = strchr(cls.downloadname, '\\')))
		*p = '/';

	length = (int)strlen(cls.downloadname);

	//normalize path
	if (cls.downloadname[0] == '.' && cls.downloadname[1] == '/')
	{
		memmove (cls.downloadname, cls.downloadname +2, length-1);
		length -=2;
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
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
			MSG_WriteString (va("download \"%s\" %i udp-zlib", cls.downloadname, len));
		else
			MSG_WriteString (va("download \"%s\" %i", cls.downloadname, len));
	}
	else
	{
		Com_Printf ("Downloading %s\n", LOG_CLIENT, cls.downloadname);

		MSG_WriteByte (clc_stringcmd);
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION)
			MSG_WriteString (va("download \"%s\" 0 udp-zlib", cls.downloadname));
		else
			MSG_WriteString (va("download \"%s\"", cls.downloadname));
	}

	MSG_EndWriting (&cls.netchan.message);

	send_packet_now = true;
	cls.downloadpending = true;

	return false;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void CL_Download_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*fp;
	char	*p;
	char	*filename;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n", LOG_CLIENT);
		return;
	}

	//Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));
	filename = Cmd_Argv(1);

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with .. (%s)\n", LOG_CLIENT, filename);
		return;
	}

	if (cls.state < ca_connected)
	{
		Com_Printf ("Not connected.\n", LOG_CLIENT);
		return;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		Com_Printf("File already exists.\n", LOG_CLIENT);
		return;
	}

	strncpy (cls.downloadname, filename, sizeof(cls.downloadname)-1);

	//r1: fix \ to /
	while ((p = strchr(cls.downloadname, '\\')))
		*p = '/';

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
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION) {
			MSG_WriteString (va("download \"%s\" %i udp-zlib", cls.downloadname, len));
		} else {
			MSG_WriteString (va("download \"%s\" %i", cls.downloadname, len));
		}
	} else {
		Com_Printf ("Downloading %s\n", LOG_CLIENT, cls.downloadname);
	
		MSG_WriteByte (clc_stringcmd);
		if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION) {
			MSG_WriteString (va("download \"%s\" 0 udp-zlib", cls.downloadname));
		} else {
			MSG_WriteString (va("download \"%s\" 0", cls.downloadname));
		}
	}
	MSG_EndWriting (&cls.netchan.message);

	send_packet_now = true;
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
		*cls.downloadtempname = 0;

		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}
#ifdef _DEBUG
		CL_RemoveFromDownloadQueue (cls.downloadname);
#endif
		cls.downloadpending = false;
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		if (!*cls.downloadtempname)
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
		unsigned short	uncompressedLen;
		byte			uncompressed[0xFFFF];

		uncompressedLen = MSG_ReadShort (&net_message);

		if (!uncompressedLen)
			Com_Error (ERR_DROP, "uncompressedLen == 0");

		ZLibDecompress (net_message_buffer + net_message.readcount, size, uncompressed, uncompressedLen, -15);
		fwrite (uncompressed, 1, uncompressedLen, cls.download);
		Com_DPrintf ("svc_zdownload(%s): %d -> %d\n", cls.downloadname, size, uncompressedLen);
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
void CL_ParseServerData (void)
{
	char	*str;
	int		i;
	
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

	if (i != ORIGINAL_PROTOCOL_VERSION && i != ENHANCED_PROTOCOL_VERSION && !cl.attractloop)
		Com_Error (ERR_DROP, "Server is using unknown protocol %d.", i);

	// game directory
	str = MSG_ReadString (&net_message);
	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir, fucking christ this is messy!
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str))) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
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
			Cvar_ForceSet ("$game", str);
		}
	}

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

	if (cls.serverProtocol == ENHANCED_PROTOCOL_VERSION && !cl.attractloop)
		cl.enhancedServer = MSG_ReadByte (&net_message);
	else
		cl.enhancedServer = 0;

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
}
/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	unsigned int	bits;
	int				newnum;

	newnum = CL_ParseEntityBits (&bits);
	es = &cl_entities[newnum].baseline;
	CL_ParseDelta (&null_entity_state, es, newnum, bits);
}

void CL_ParseZPacket (void)
{
	byte buff_in[MAX_MSGLEN];
	byte buff_out[0xFFFF];

	sizebuf_t sb, old;

	short compressed_len = MSG_ReadShort (&net_message);
	short uncompressed_len = MSG_ReadShort (&net_message);
	
	if (uncompressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: uncompressed_len <= 0");

	if (compressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: compressed_len <= 0");

	//buff_in = Z_Malloc (compressed_len);
	//buff_out = Z_Malloc (uncompressed_len);

	MSG_ReadData (&net_message, buff_in, compressed_len);

	SZ_Init (&sb, buff_out, uncompressed_len);
	sb.cursize = ZLibDecompress (buff_in, compressed_len, buff_out, uncompressed_len, -15);

	old = net_message;
	net_message = sb;

	/*for (;;)
	{
		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
			break;

		switch (cmd) {
			case svc_configstring:
				CL_ParseConfigString ();
				break;
			case svc_spawnbaseline:
				CL_ParseBaseline ();
				break;
			default:
				Com_Error (ERR_DROP, "CL_ParseZPacket: unhandled command 0x%x!", cmd);
				break;
		}
	}*/

	CL_ParseServerMessage ();

	net_message = old;

	//Z_Free (buff_in);
	//Z_Free (buff_out);

	Com_DPrintf ("Got a ZPacket, %d->%d\n", uncompressed_len + 4, compressed_len);
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
		if (!(*t > 32 && *t < 128))
		{
			i = -1;
			break;
		}
		t++;
	}

	if (s[0] == 0 || i == -1)
	{
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
			*t = 0;
		}

		//strcpy (original_model_name, model_name);

		// isolate the skin name
		Q_strncpy (skin_name, s + strlen(model_name) + 1, sizeof(skin_name)-1);
		//strcpy (original_skin_name, s + strlen(model_name) + 1);

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = re.RegisterModel (model_filename);
		if (!ci->model)
		{
			ci->deferred = true;
			//if (!CL_CheckOrDownloadFile (model_filename))
			//	return;
#ifdef _DEBUG
			CL_AddToDownloadQueue (model_filename);
#endif
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
#ifdef _DEBUG
			CL_AddToDownloadQueue (skin_filename);
#endif
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
#ifdef _DEBUG
			CL_AddToDownloadQueue (ci->iconname);
#endif
			//ci->icon = re.RegisterPic ("/players/male/grunt_i.pcx");
		}
	}

	// weapon file
	for (i = 0; i < num_cl_weaponmodels; i++) {
		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
		ci->weaponmodel[i] = re.RegisterModel(weapon_filename);
		if (!ci->weaponmodel[i]) {
			//Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", original_model_name, cl_weaponmodels[i]);
			ci->deferred = true;
#ifdef _DEBUG
			CL_AddToDownloadQueue (weapon_filename);
#endif
		}
		if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0) {
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
	int		i;
	int		length;
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
	length = (int)strlen(s);

	if (length >= (sizeof(cl.configstrings[0]) * (MAX_CONFIGSTRINGS-i)) - 1)
		Com_Error (ERR_DROP, "CL_ParseConfigString: configstring %d exceeds available space", i);

	//r1: don't allow basic things to overflow
	if ((i < CS_STATUSBAR && i >= CS_AIRACCEL) && (i < CS_GENERAL))
	{
		if (length >= MAX_QPATH)
			Com_Printf ("WARNING: Configstring %d of length %d exceeds MAX_QPATH.\n", LOG_CLIENT|LOG_WARNING, i, length);
		Q_strncpy (cl.configstrings[i], s, sizeof(cl.configstrings[i])-1);
	}
	else
	{
		strcpy (cl.configstrings[i], s);
	}

	// do something apropriate
	if (i == CS_AIRACCEL)
	{
		pm_airaccelerate = atof(cl.configstrings[CS_AIRACCEL]);
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
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		//r1: hack to avoid parsing non-skins from mods that overload CS_PLAYERSKINS
		//FIXME: how reliable is CS_MAXCLIENTS?
		i -= CS_PLAYERSKINS;
		if ((cl.configstrings[CS_MAXCLIENTS][0] && i < atoi(cl.configstrings[CS_MAXCLIENTS])) || Com_ServerState() == ss_demo)
		{
			if (cl.refresh_prepped && strcmp(olds, s))
				CL_ParseClientinfo (i);
		}
		else
		{
			Com_DPrintf ("CL_ParseConfigString: Ignoring out-of-range playerskin %d (%s)\n", i, MakePrintable(s));
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
	sound_num = MSG_ReadByte (&net_message);

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
		ofs = MSG_ReadByte (&net_message) / 1000.0;
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		ent = channel>>3;
		if (ent > MAX_EDICTS)
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


void SHOWNET(char *s)
{
	if (cl_shownet->intvalue>=2)
		Com_Printf ("%3i:%s\n", LOG_CLIENT, net_message.readcount-1, s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i;
	qboolean	gotFrame;
//
// if recording demos, copy the message out
//
	if (cl_shownet->intvalue == 1)
		Com_Printf ("%i ", LOG_CLIENT, net_message.cursize);
	else if (cl_shownet->intvalue >= 2)
		Com_Printf ("------------------\n", LOG_CLIENT);

	serverPacketCount++;
	gotFrame = false;

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

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

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
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 ();
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			strncpy (cl.layout, s, sizeof(cl.layout)-1);
			break;

		case svc_inventory:
			CL_ParseInventory ();
			break;

		case svc_nop:
			break;
			
		case svc_disconnect:
			//uuuuugly...
			if (cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION && cls.realtime - cls.connect_time < 30000)
			{
				Com_Printf ("Disconnected by server, assuming protocol mismatch. Reconnecting with protocol 34.\n", LOG_CLIENT);
				CL_Disconnect(false);
				cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
				CL_Reconnect_f ();
				return;
			}
			else
			{
				Com_Error (ERR_DISCONNECT, "Server disconnected\n");
			}
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n", LOG_CLIENT);
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			break;

		case svc_sound:
			CL_ParseStartSoundPacket();
			break;

		case svc_print:
			i = MSG_ReadByte (&net_message);
			s = MSG_ReadString (&net_message);
			if (i == PRINT_CHAT)
			{
				if (CL_IgnoreMatch (s))
					break;

				S_StartLocalSound ("misc/talk.wav");
				if (cl_filterchat->intvalue)
				{
					StripHighBits(s, (int)cl_filterchat->intvalue == 2);
					strcat (s, "\n");
				}
				con.ormask = 128;

				//r1: change !p_version to !version since p is for proxies
				if (strstr (s, ": !r1q2_version") ||
					strstr (s, ": !version") &&
					(cls.lastSpamTime == 0 || cls.realtime > cls.lastSpamTime + 300000))
					cls.spamTime = cls.realtime + random() * 1500; 
			}

			// for getting debug info overnight :)
/*#ifdef _DEBUG
			{
				char *p;
				p = strstr (s, "was machinegunned by ");
				if (p)
				{
					char *q;
					p += 21;
					q = strstr (p, ".");
					if (q) *q = 0;
					Cbuf_AddText ("chase ");
					Cbuf_AddText (p);
					Cbuf_AddText ("\n");
				}
			}
#endif*/
			Com_Printf ("%s", LOG_CLIENT|LOG_CHAT, s);
			con.ormask = 0;
			break;

		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf ("stufftext: %s\n", s);
			Cbuf_AddText (s);
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline ();
			break;

		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (&net_message));
			break;

		case svc_download:
			CL_ParseDownload (false);
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;

		case svc_frame:
			CL_ParseFrame ();
			gotFrame = true;
			break;

		// ************** r1q2 specific BEGIN ****************
		case svc_zpacket:
			CL_ParseZPacket();
			break;

		case svc_zdownload:
			CL_ParseDownload(true);
			break;
		// ************** r1q2 specific END ******************

		default:
			if (developer->intvalue)
			{
				Com_Printf ("Unknown command char %d, ignoring!!\n", LOG_CLIENT, cmd);
			}
			else
			{
				if (cls.serverProtocol != ORIGINAL_PROTOCOL_VERSION && cls.realtime - cls.connect_time < 30000)
				{
					Com_Printf ("Unknown command byte %d, assuming protocol mismatch. Reconnecting with protocol 34.\n", LOG_CLIENT, cmd);
					CL_Disconnect(false);
					cls.serverProtocol = ORIGINAL_PROTOCOL_VERSION;
					CL_Reconnect_f ();
					return;
				}
				Com_Error (ERR_DROP,"CL_ParseServerMessage: Unknown command byte %d (0x%.2x)", cmd, cmd);
			}
			break;

		}
	}

	if (!gotFrame)
		noFrameFromServerPacket++;
	else
		noFrameFromServerPacket = 0;
}
