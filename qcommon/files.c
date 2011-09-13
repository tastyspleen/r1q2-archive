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

#include "qcommon.h"
#include "redblack.h"
#include "unzip.h"

#include <sys/types.h>
#include <sys/stat.h>
/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

//
// in memory
//

typedef struct
{
	union filehandle_type
	{
		FILE			*handle;
		unzFile			*zhandle;
	} h;
	uint32			length;
	uint32			refcount;
} fshandle_t;

typedef struct
{
	char			name[56];
	uint32			filepos;
	uint32			filelen;
} packfile_t;

typedef struct
{
	uint32			offset;
} zpackfile_t;

typedef enum
{
	PAK_QUAKE,
	PAK_ZIP,
} packtype_t;

typedef struct pack_s
{
	char			filename[MAX_OSPATH];
	union packhandle_type
	{
		FILE			*handle;
		unzFile			*zhandle;
	} h;
	int				numfiles;
	//packfile_t		*files;
	struct rbtree	*rb;
	packtype_t		type;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
cvar_t	*fs_gamedirvar;
cvar_t	*fs_cache;
cvar_t	*fs_noextern;

typedef struct filelink_s
{
	struct filelink_s	*next;
	char	*from;
	int		fromlength;
	char	*to;
} filelink_t;

static filelink_t	*fs_links;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

static searchpath_t	*fs_searchpaths;
static searchpath_t	*fs_base_searchpaths;	// without gamedirs

static const char *current_filename;

/*

All of Quake's data access is through a hierchal file system, but the contents of
the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game
directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This
can be overridden with the "-basedir" command line parm to allow code debugging in a
different directory.  The base directory is only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all
generated files (savegames, screenshots, demos, config files) will be saved to. 
This can be overridden with the "-game" command line parameter.  The game directory
can never be changed while quake is executing.  This is a precacution against having a
malicious server instruct clients to write files over areas they shouldn't.

*/


/*
================
FS_filelength
================
*/
static int FS_filelength (FILE *f)
{
//	int		pos;
	int		end;

	//pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	rewind (f);
	//fseek (f, pos, SEEK_SET);

	return end;
}

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


/*
==============
FS_FCloseFile

For some reason, other dll's can't just call fclose()
on files returned by FS_FOpenFile...
==============
*/
void EXPORT FS_FCloseFile (FILE *f)
{
	fclose (f);
}


// RAFAEL
/*
	Developer_searchpath
*/
int	Developer_searchpath (void)
{
	searchpath_t	*search;
	

	for (search = fs_searchpaths ; search ; search = search->next)
	{
		if (strstr (search->filename, "xatrix"))
			return 1;

		if (strstr (search->filename, "rogue"))
			return 2;
/*
		start = strchr (search->filename, ch);

		if (start == NULL)
			continue;

		if (strcmp (start ,"xatrix") == 0)
			return (1);
*/
	}
	return (0);

}

#define BTREE_SEARCH 1
//#define HASH_CACHE 1
//#define MAGIC_BTREE 1

#if HASH_CACHE || MAGIC_BTREE
uint32 hashify (char *S)
{
  uint32 hash_PeRlHaSh = 0;
  char c;
  while (*S) {
	  c = fast_tolower(*S++);
	  hash_PeRlHaSh = hash_PeRlHaSh * 33 + (c);
  }
  return hash_PeRlHaSh + (hash_PeRlHaSh >> 5);
}
#endif

typedef struct fscache_s fscache_t;

struct fscache_s
{
	char		filename[MAX_QPATH];
	char		filepath[MAX_OSPATH];
	uint32		filelen;
	uint32		fileseek;
	pack_t		*pak;
#ifdef HASH_CACHE
	fscache_t	*next;
	uint32		hash;
#endif
};

static struct rbtree *rb;

#ifdef MAGIC_BTREE
static int _compare(const void *pa, const void *pb)
{
	if ((int *)pa > (int *)pb)
		return 1;
	else if ((int *)pa < (int *)pb)
		return -1;
	return 0;
}
#endif

void FS_InitCache (void)
{
#ifdef MAGIC_BTREE
	rb = rbinit (_compare, 1);
#else
#ifdef LINUX
	rb = rbinit ((int (EXPORT *)(const void *, const void *))strcmp, 0);
#else
	rb = rbinit ((int (EXPORT *)(const void *, const void *))Q_stricmp, 0);
#endif
#endif

	if (!rb)
		Com_Error (ERR_FATAL, "FS_InitCache: rbinit failed"); 
}

#ifdef HASH_CACHE
static fscache_t fscache;
#endif

static void RB_Purge (const struct rbtree *r)
{
	RBLIST *rblist;
	const void *val;
	void *data;
	void *ptr;

	if ((rblist=rbopenlist(rb)))
	{
		while((val=rbreadlist(rblist)))
		{
			data = rbfind (val, rb);
			ptr = *(void **)data;
			rbdelete (val, rb);

			if (ptr)
				Z_Free (ptr);
		}
	}

	rbcloselist(rblist);
	rbdestroy (rb);

	FS_InitCache();
}

void FS_FlushCache (void)
{
#ifdef HASH_CACHE
	fscache_t *last = NULL, *temp;

	temp = &fscache;
	temp = temp->next;

	if (temp) {
		while (temp->next) {
			last = temp->next;
			Z_Free (temp);
			temp = last;
		}

		fscache.next = NULL;

		Z_Free (temp);
	}

	memset (&fscache, 0, sizeof(fscache));
#endif
	RB_Purge (rb);
}

static void FS_Stats_f (void)
{
#if BTREE_SEARCH || MAGIC_BTREE
	int			i;
	int			j;
	int			k;
	const void	*val;
#endif
#ifdef HASH_CACHE
	fscache_t *temp;
#endif

#if BTREE_SEARCH || MAGIC_BTREE
	i = 0;
	j = 1;
	k = 1;

	for(val=rblookup(RB_LUFIRST, NULL, rb); val!=NULL; val=rblookup(RB_LUNEXT, val, rb))
		i++;

	while (j < i)
	{
		k++;
		j <<= 1;
	}
	Com_Printf ("%d entries in binary search tree cache (est. height %d).\n", LOG_GENERAL, i, k);
#endif

#ifdef HASH_CACHE
	temp = &fscache;
	temp = temp->next;

	i = 0;

	if (temp)
	{
		while (temp->next)
		{
			i++;
			temp = temp->next;
		}
	}

	Com_Printf ("%d entries in linked list hash cache.\n", LOG_GENERAL, i);
#endif
}

#if BTREE_SEARCH
static void FS_AddToCache (const char *path, uint32 filelen, uint32 fileseek, const char *filename, pack_t *pak)
{
	void		**newitem;
	fscache_t	*cache;

	if (!q2_initialized)
		return;

	cache = Z_TagMalloc (sizeof(fscache_t), TAGMALLOC_FSCACHE);
	cache->filelen = filelen;
	cache->fileseek = fileseek;
	cache->pak = pak;

	if (path)
		strncpy (cache->filepath, path, sizeof(cache->filepath)-1);
	else
		cache->filepath[0] = 0;

	strncpy (cache->filename, filename, sizeof(cache->filename)-1);

	newitem = rbsearch (cache->filename, rb);
	*newitem = cache;
}
#endif

#if MAGIC_BTREE

typedef struct magic_s
{
	struct magic_s	*next;
	fscache_t		*entry;
} magic_t;

static void FS_AddToCache (char *path, uint32 filelen, uint32 fileseek, char *filename, uint32 hash)
{
	void		**newitem;
	fscache_t	*cache;
	magic_t		*magic;

	cache = Z_TagMalloc (sizeof(fscache_t), TAGMALLOC_FSCACHE);
	cache->filelen = filelen;
	cache->fileseek = fileseek;

	if (path)
		strncpy (cache->filepath, path, sizeof(cache->filepath)-1);
	else
		cache->filepath[0] = 0;

	strncpy (cache->filename, filename, sizeof(cache->filename)-1);

	newitem = rbfind ((void *)hash, rb);
	if (newitem)
	{
		magic = *(magic_t **)newitem;
		while (magic->next)
			magic = magic->next;
		magic->next = Z_TagMalloc (sizeof(magic_t), TAGMALLOC_FSCACHE);
		magic = magic->next;
		magic->entry = cache;
		magic->next = NULL;
	}
	else
	{
		newitem = rbsearch ((void *)hash, rb);
		magic = Z_TagMalloc (sizeof(magic_t), TAGMALLOC_FSCACHE);
		magic->entry = cache;
		magic->next = NULL;
		*newitem = magic;
	}
}
#endif

#if HASH_CACHE
static void FS_AddToCache (uint32 hash, char *path, uint32 filelen, uint32 fileseek, fscache_t *cache, char *filename)
{
	cache->next = Z_TagMalloc (sizeof(fscache_t), TAGMALLOC_FSCACHE);
	cache = cache->next;
	cache->filelen = filelen;
	cache->hash = hash;
	cache->next = NULL;
	cache->fileseek = fileseek;

	if (path)
		strncpy (cache->filepath, path, sizeof(cache->filepath)-1);
	else
		cache->filepath[0] = 0;

	strncpy (cache->filename, filename, sizeof(cache->filename)-1);
}
#endif

void FS_WhereIs_f (void)
{
	char			*filename;
	searchpath_t	*search;
	pack_t			*pak;
	filelink_t		*link;
	char			netpath[MAX_OSPATH];
	char			lowered[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Purpose: Find where a file is being loaded from on the filesystem.\n"
					"Syntax : whereis <path>\n"
					"Example: whereis maps/q2dm1.bsp\n", LOG_GENERAL);
		return;
	}

	filename = Cmd_Argv(1);

	// check for links firstal
	if (!fs_noextern->intvalue)
	{
		for (link = fs_links ; link ; link=link->next)
		{
			if (!strncmp (filename, link->from, link->fromlength))
			{
				int	len;
				Com_sprintf (netpath, sizeof(netpath), "%s%s",link->to, filename+link->fromlength);
				len = Sys_FileLength (netpath);
				if (len != -1)
				{
					Com_Printf ("%s is found on disk as %s (using linkpath), %d bytes.\n", LOG_GENERAL, Cmd_Argv(1), netpath, len);
				}
				else
				{
					Com_Printf ("%s is not found.\n", LOG_GENERAL, Cmd_Argv(1));
				}
				return;
			}
		}
	}

	Q_strncpy (lowered, filename, sizeof(lowered)-1);
	fast_strlwr (lowered);

	for (search = fs_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			packfile_t	*entry;

			//r1: optimized btree search
			pak = search->pack;

			entry = rbfind (lowered, pak->rb);

			if (entry)
			{
				entry = *(packfile_t **)entry;
				Com_Printf ("%s is found in pakfile %s as %s, %d bytes.\n", LOG_GENERAL, Cmd_Argv(1), pak->filename, entry->name, entry->filelen);
				return;
			}
		}
		else if (!fs_noextern->intvalue)
		{
			int filelen;
			// check a file in the directory tree
			
			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);

			filelen = Sys_FileLength (netpath);
			if (filelen == -1)
				continue;

			Com_Printf ("%s is found on disk as %s, %d bytes.\n", LOG_GENERAL, Cmd_Argv(1), netpath, filelen);
			return;
		}
	}

	Com_Printf ("%s is not found.\n", LOG_GENERAL, Cmd_Argv(1));
}

/*
===========
FS_FOpenFile

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/

int EXPORT FS_FOpenFile (const char *filename, FILE **file, handlestyle_t openHandle, qboolean *closeHandle)
{
	fscache_t		*cache;
	searchpath_t	*search;
	pack_t			*pak;
	filelink_t		*link;
	char			netpath[MAX_OSPATH];
	char			lowered[MAX_QPATH];

	// check for links firstal
	if (!fs_noextern->intvalue)
	{
		for (link = fs_links ; link ; link=link->next)
		{
			if (!strncmp (filename, link->from, link->fromlength))
			{
				Com_sprintf (netpath, sizeof(netpath), "%s%s",link->to, filename+link->fromlength);
				if (openHandle != HANDLE_NONE)
				{
					*file = fopen (netpath, "rb");
					if (*file)
					{	
						Com_DPrintf ("link file: %s\n",netpath);
						*closeHandle = true;
						return FS_filelength (*file);
					}
					return -1;
				}
				else
				{
					return Sys_FileLength (netpath);
				}
			}
		}
	}

#ifdef BTREE_SEARCH
	cache = rbfind (filename, rb);
	if (cache)
	{
		cache = *(fscache_t **)cache;
		if (cache->filepath[0] == 0)
		{
			*file = NULL;
			return -1;
		}
#ifdef _DEBUG
		Com_DPrintf ("File '%s' found in cache: %s\n", filename, cache->filepath);
#endif
		if (openHandle != HANDLE_NONE)
		{
			if (cache->pak)
			{
				if (openHandle == HANDLE_DUPE)
				{
					*file = fopen (cache->pak->filename, "rb");
					if (!*file)
					{
						Com_Printf ("WARNING: Cached pak '%s' failed to open! Did you delete it?\n", LOG_WARNING|LOG_GENERAL, cache->pak->filename);
						rbdelete (filename, rb);
						return -1;
					}
					*closeHandle = true;
				}
				else
				{
					*file = cache->pak->h.handle;
					*closeHandle = false;
				}
			}
			else
			{
				*file = fopen (cache->filepath, "rb");
				if (!*file)
				{
					Com_Printf ("WARNING: Cached file '%s' failed to open! Did you delete it?\n", LOG_WARNING|LOG_GENERAL, cache->filepath);
					rbdelete (filename, rb);
					return -1;
				}
					//Com_Error (ERR_FATAL, "Couldn't open %s (cached)", cache->filepath);	

				*closeHandle = true;
			}
			if (cache->fileseek && fseek (*file, cache->fileseek, SEEK_SET))
				Com_Error (ERR_FATAL, "Couldn't seek to offset %u in %s (cached)", cache->fileseek, cache->filepath);
		}
		return cache->filelen;
	}

#elif HASH_CACHE
	hash = hashify (filename);

	cache = &fscache;

	while (cache->next)
	{ 
		cache = cache->next;
		if (cache->hash == hash && !Q_stricmp (cache->filename, filename))
		{
			Com_Printf (" (cached) ", LOG_GENERAL);
			if (cache->filepath[0] == 0)
			{
				*file = NULL;
				return -1;
			}
			*file = fopen (cache->filepath, "rb");
			if (!*file)
				Com_Error (ERR_FATAL, "Couldn't open %s", cache->filepath);	
			fseek (*file, cache->fileseek, SEEK_SET);
			return cache->filelen;
		}
	}
#elif MAGIC_BTREE
	{
		magic_t *magic;

		hash = hashify (filename);

		magic = rbfind ((void *)hash, rb);
		if (magic)
		{
			magic = *(magic_t **)magic;

			do
			{
				cache = magic->entry;
				if (!Q_stricmp (cache->filename, filename))
				{
					if (cache->filepath[0] == 0)
					{
						*file = NULL;
						return -1;
					}
					*file = fopen (cache->filepath, "rb");
					if (!*file)
						Com_Error (ERR_FATAL, "Couldn't open %s", cache->filepath);	
					fseek (*file, cache->fileseek, SEEK_SET);
					return cache->filelen;
				}

				magic = magic->next;
			} while (magic);
		}
	}
#endif

#ifdef _DEBUG
	Com_DPrintf ("File '%s' not found in cache, searching fs_searchpaths\n", filename);
#endif

	Q_strncpy (lowered, filename, sizeof(lowered)-1);
	fast_strlwr (lowered);

	for (search = fs_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
//			char		*lower;
			packfile_t	*entry;

			//r1: optimized btree search
			pak = search->pack;

			if (pak->type == PAK_QUAKE)
			{
				entry = rbfind (lowered, pak->rb);

				if (entry)
				{
					entry = *(packfile_t **)entry;

	#ifdef _DEBUG
					Com_DPrintf ("File '%s' found in %s, (%s)\n", filename, pak->filename, entry->name);
	#endif
					if (openHandle != HANDLE_NONE)
					{
						//*file = fopen (pak->filename, "rb");
						if (openHandle == HANDLE_DUPE)
						{
							*file = fopen (pak->filename, "rb");
							*closeHandle = true;	
						}
						else
						{
							*file = pak->h.handle;
							*closeHandle = false;
						}
						//if (!*file)
						//	Com_Error (ERR_FATAL, "Couldn't reopen pak file %s", pak->filename);	

						if (fseek (*file, entry->filepos, SEEK_SET))
							Com_Error (ERR_FATAL, "Couldn't seek to offset %u for %s in %s", entry->filepos, entry->name, pak->filename);
					}

					if (fs_cache->intvalue & 1)
					{
	#if BTREE_SEARCH
						FS_AddToCache (pak->filename, entry->filelen, entry->filepos, filename, pak);
	#elif HASH_CACHE
						FS_AddToCache (hash, pak->filename, entry->filelen, entry->filepos, cache, filename);
	#elif MAGIC_BTREE
						FS_AddToCache (pak->filename, entry->filelen, entry->filepos, filename, hash);
	#endif
					}

					return entry->filelen;
				}
			}
		}
		else if (!fs_noextern->intvalue)
		{
			struct stat	statInfo;
			int filelen;
			// check a file in the directory tree
			
			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);

			if (openHandle == HANDLE_NONE)
			{
				filelen = Sys_FileLength (netpath);
				if (filelen == -1)
					continue;
				
				if (fs_cache->intvalue & 4)
					FS_AddToCache (netpath, filelen, 0, filename, NULL);

				return filelen;
			}

			//fix for moronic implementations that allow fopen (FILE OPEN) to open a directory
			//(this means you linux and krew)
			if (stat (netpath, &statInfo))
				continue;

			if (statInfo.st_mode & S_IFDIR)
			{
				Com_Printf ("WARNING: Tried to open a directory as a file: %s\n", LOG_WARNING, netpath);
				continue;
			}

			*file = fopen (netpath, "rb");

			if (!*file)
				continue;

			*closeHandle = true;
			
			Com_DPrintf ("FindFile: %s\n",netpath);

			filelen = FS_filelength (*file);
			if (fs_cache->intvalue & 4)
			{
#if BTREE_SEARCH
				FS_AddToCache (netpath, filelen, 0, filename, NULL);
#elif HASH_CACHE
				FS_AddToCache (hash, netpath, filelen, 0, cache, filename);
#elif MAGIC_BTREE
				FS_AddToCache (netpath, filelen, 0, filename, hash);
#endif
			}
			return filelen;
		}
		
	}
	
	Com_DPrintf ("FindFile: can't find %s\n", filename);

	if (fs_cache->intvalue & 2)
	{
#if BTREE_SEARCH
		FS_AddToCache (NULL, 0, 0, filename, NULL);
#elif HASH_CACHE
		FS_AddToCache (hash, NULL, 0, 0, cache, filename);
#elif MAGIC_BTREE
		FS_AddToCache (NULL, 0, 0, filename, hash);
#endif
	}
	
	*file = NULL;
	return -1;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
#ifdef CD_AUDIO
#include "../client/cdaudio.h"
#endif

#define	MAX_READ	0x40000		// read in blocks of 64k
								// r1: bumped to 256k.
void EXPORT FS_Read (void *buffer, int len, FILE *f)
{
	int		block, remaining;
	size_t	read;
	byte	*buf;
#ifdef CD_AUDIO
	int		tries = 0;
#endif

	Q_assert (len != 0);

	buf = (byte *)buffer;

	// read in chunks for progress bar
	remaining = len;

	while (remaining)
	{
		block = remaining;
		if (block > MAX_READ)
			block = MAX_READ;

		read = fread (buf, block, 1, f);
		if (read == 0)
		{
			// we might have been trying to read from a CD
#ifdef CD_AUDIO
			if (!tries)
			{
				tries = 1;
				CDAudio_Stop();
			}
			else
#endif
			{
				if (ferror (f))
				{
					Com_Error (ERR_FATAL, "FS_Read(%d): Read error on '%s'. Did you forget to empty the filesystem cache after modifying a file?", len, current_filename);
				}
				else
				{
					Com_Printf ("WARNING: Incomplete read of %d bytes from '%s'. Did you forget to empty the filesystem cache after modifying a file?\n", LOG_WARNING, len, current_filename);
					return;
				}
			}
		}

		// do some progress bar thing here...

		remaining -= block;
		buf += block;
	}
}

/*
============
FS_LoadFile

Filename are reletive to the quake search path
a null buffer will just return the file length without loading
============
*/
int EXPORT FS_LoadFile (const char *path, void /*@out@*/ /*@null@*/**buffer)
{
	FILE		*h;
	byte		*buf;
	int			len;
	qboolean	closeHandle;
	// look for it in the filesystem or pack files
	//START_PERFORMANCE_TIMER;
	//Com_Printf ("%s... ", path);
	len = FS_FOpenFile (path, &h, buffer ? HANDLE_OPEN : HANDLE_NONE, &closeHandle);
	//STOP_PERFORMANCE_TIMER;

	//Com_Printf ("TOTAL SO FAR: %.5f\n", totalTime);

	if (len == -1)
	{
		if (buffer)
			*buffer = NULL;
		return -1;
	}
	
	if (!buffer)
		return len;

	if (!len)
	{
		fclose (h);
		Com_Printf ("WARNING: 0 byte file: %s\n", LOG_GENERAL|LOG_WARNING, path);
		*buffer = CopyString ("", TAGMALLOC_FSLOADFILE);
		return 0;
	}

	buf = Z_TagMalloc(len, TAGMALLOC_FSLOADFILE);
	*buffer = buf;
	current_filename = path;
	FS_Read (buf, len, h);
	current_filename = "unknown";

	if (closeHandle)
		fclose (h);

	return len;
}


/*
=============
FS_FreeFile
=============
*/
void EXPORT FS_FreeFile (void *buffer)
{
	Z_Free (buffer);
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static pack_t /*@null@*/ *FS_LoadPackFile (const char *packfile, const char *ext)
{
	
	int				i;
	void			**newitem;
	pack_t			*pack = NULL;
	packfile_t		*info;

	if (!strcmp (ext, "pak"))
	{
		unsigned		pakLen;
		int				numpackfiles;
		FILE			*packhandle;
		dpackheader_t	header;

		packhandle = fopen(packfile, "rb");

		if (!packhandle)
			return NULL;

		fseek (packhandle, 0, SEEK_END);
		pakLen = ftell (packhandle);
		rewind (packhandle);

		if (fread (&header, sizeof(header), 1, packhandle) != 1)
			Com_Error (ERR_FATAL, "FS_LoadPackFile: Couldn't read pak header from %s", packfile);

		if (LittleLong(header.ident) != IDPAKHEADER)
			Com_Error (ERR_FATAL, "FS_LoadPackFile: %s is not a valid pak file.", packfile);
		
	#if YOU_HAVE_A_BROKEN_COMPUTER
		header.dirofs = LittleLong (header.dirofs);
		header.dirlen = LittleLong (header.dirlen);
	#endif

		if (header.dirlen % sizeof(packfile_t))
			Com_Error (ERR_FATAL, "FS_LoadPackFile: Bad pak file %s (directory length %u is not a multiple of %d)", packfile, header.dirlen, (int)sizeof(packfile_t));

		numpackfiles = header.dirlen / sizeof(packfile_t);

		if (numpackfiles > MAX_FILES_IN_PACK)
			//Com_Error (ERR_FATAL, "FS_LoadPackFile: packfile %s has %i files (max allowed %d)", packfile, numpackfiles, MAX_FILES_IN_PACK);
			Com_Printf ("WARNING: Pak file %s has %i files (max allowed %d) - may not be compatible with other clients\n", LOG_GENERAL, packfile, numpackfiles, MAX_FILES_IN_PACK);

		if (!numpackfiles)
		{
			fclose (packhandle);
			Com_Printf ("WARNING: Empty packfile %s\n", LOG_GENERAL|LOG_WARNING, packfile);
			return NULL;
		}

		//newfiles = Z_TagMalloc (numpackfiles * sizeof(packfile_t), TAGMALLOC_FSLOADPAK);
		info = Z_TagMalloc (numpackfiles * sizeof(packfile_t), TAGMALLOC_FSLOADPAK);

		if (fseek (packhandle, header.dirofs, SEEK_SET))
			Com_Error (ERR_FATAL, "FS_LoadPackFile: fseek() to offset %u in %s failed. Pak file is possibly corrupt.", header.dirofs, packfile);

		if ((int)fread (info, 1, header.dirlen, packhandle) != header.dirlen)
			Com_Error (ERR_FATAL, "FS_LoadPackFile: Error reading packfile directory from %s (failed to read %u bytes at %u). Pak file is possibly corrupt.", packfile, header.dirofs, header.dirlen);

		pack = Z_TagMalloc (sizeof (pack_t), TAGMALLOC_FSLOADPAK);
		pack->type = PAK_QUAKE;
		pack->rb = rbinit ((int (EXPORT *)(const void *, const void *))strcmp, numpackfiles);

		//entry = Z_TagMalloc (sizeof(packfile_t) * numpackfiles, TAGMALLOC_FSLOADPAK);

		for (i=0 ; i<numpackfiles ; i++)
		{
			fast_strlwr (info[i].name);
#if YOU_HAVE_A_BROKEN_COMPUTER
			info[i].filepos = LittleLong(info[i].filepos);
			info[i].filelen = LittleLong(info[i].filelen);
#endif
			if (info[i].filepos + info[i].filelen >= pakLen)
				Com_Error (ERR_FATAL, "FS_LoadPackFile: File '%.64s' in pak file %s has illegal offset %u past end of file %u. Pak file is possibly corrupt.", MakePrintable (info[i].name, 0), packfile, info[i].filepos, pakLen);
			
			newitem = rbsearch (info[i].name, pack->rb);
			*newitem = &info[i];
		}

		Q_strncpy (pack->filename, packfile, sizeof(pack->filename)-1);

		pack->h.handle = packhandle;
		pack->numfiles = numpackfiles;

		Com_Printf ("Added packfile %s (%i files)\n", LOG_GENERAL,  packfile, numpackfiles);
	}
#ifndef NO_ZLIB
	else if (!strcmp (ext, "pkz"))
	{
		unzFile			f;
		unz_global_info	zipinfo;
		char			zipFileName[56];
		unz_file_info	fileInfo;

		f = unzOpen (packfile);
		if (!f)
			return NULL;

		if (unzGetGlobalInfo (f, &zipinfo) != UNZ_OK)
			Com_Error (ERR_FATAL, "FS_LoadPackFile: Couldn't read .zip info from '%s'", packfile);

		info = Z_TagMalloc (zipinfo.number_entry * sizeof(*info), TAGMALLOC_FSLOADPAK);

		pack = Z_TagMalloc (sizeof (pack_t), TAGMALLOC_FSLOADPAK);
		pack->type = PAK_ZIP;
		pack->rb = rbinit ((int (EXPORT *)(const void *, const void *))strcmp, zipinfo.number_entry);

		if (unzGoToFirstFile (f) != UNZ_OK)
			Com_Error (ERR_FATAL, "FS_LoadPackFile: Couldn't seek to first .zip file in '%s'", packfile);

		zipFileName[sizeof(zipFileName)-1] = 0;
		i = 0;
		do
		{
			if (unzGetCurrentFileInfo (f, &fileInfo, zipFileName, sizeof(zipFileName)-1, NULL, 0, NULL, 0) == UNZ_OK)
			{
				//directory, ignored
				if (fileInfo.external_fa & 16)
					continue;
				strcpy (info[i].name, zipFileName);
				info[i].filepos = unzGetOffset (f);
				info[i].filelen = fileInfo.uncompressed_size;
				newitem = rbsearch (info[i].name, pack->rb);
				*newitem = &info[i];
				i++;
			}
		} while (unzGoToNextFile (f) == UNZ_OK);

		pack->h.zhandle = f;
		Com_Printf ("Added zpackfile %s (%i files)\n", LOG_GENERAL,  packfile, i);
	}
#endif
	else
	{
		Com_Error (ERR_FATAL, "FS_LoadPackFile: Unknown type %s", ext);
	}

	return pack;
}

static int EXPORT pakcmp (const void *a, const void *b)
{
	if (*(int *)a > *(int *)b)
		return 1;
	else if (*(int *)a == *(int *)b)
		return 0;
	return -1;
}

static int EXPORT filecmp (const void *a, const void *b)
{
   return strcmp (*(char**)a, *(char**)b);
}

static void FS_LoadPaks (const char *dir, const char *ext)
{
	int				i;
	int				total;
	int				totalpaks;
	size_t			pakmatchlen;

	char			pakfile[MAX_OSPATH];
	char			pakmatch[MAX_OSPATH];
	char			*s;

	char			*filenames[4096];
	int				pakfiles[1024];

	pack_t			*pak;
	searchpath_t	*search;

	//r1: load all *.pak files
	Com_sprintf (pakfile, sizeof(pakfile), "%s/*.%s", dir, ext);
	Com_sprintf (pakmatch, sizeof(pakmatch), "%s/pak", dir);
	pakmatchlen = strlen(pakmatch);

	total = 0;
	totalpaks = 0;

	if ((s = Sys_FindFirst (pakfile, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM)))
	{
		while (s)
		{
			i = (int)strlen (s);
			if (*(s+(i-4)) == '.' && !Q_stricmp (s+(i-3), ext))
			{
				if (!Q_strncasecmp (s, pakmatch, pakmatchlen))
				{
					pakfiles[totalpaks++] = atoi(s+pakmatchlen);
				}
				else
				{
					filenames[total++] = strdup(s);
					//filenames[total] = alloca(strlen(s)+1);
					//strcpy (filenames[total], s);
					//total++;
				}
			}

			s = Sys_FindNext (0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);
		}
	}

	Sys_FindClose ();

	//sort for filenames designed to override earlier pak files
	qsort (filenames, total, sizeof(filenames[0]), filecmp);
	qsort (pakfiles, totalpaks, sizeof(pakfiles[0]), pakcmp);

	//r1: load pak*.pak first
	for (i = 0; i < totalpaks; i++)
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%d.%s", dir, pakfiles[i], ext);
		pak = FS_LoadPackFile (pakfile, ext);
		if (pak)
		{
			search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
			search->pack = pak;
			search->filename[0] = 0;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
	}

	//now the rest of them
	for (i = 0; i < total; i++)
	{
		pak = FS_LoadPackFile (filenames[i], ext);
		if (pak)
		{
			search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
			search->pack = pak;
			search->filename[0] = 0;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
		free (filenames[i]);
	}
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ... 
================
*/
static void FS_AddGameDirectory (const char *dir)
{
	searchpath_t	*search;

	Q_strncpy (fs_gamedir, dir, sizeof(fs_gamedir)-1);

	Com_DPrintf ("FS_AddGameDirectory: Added '%s'\n", dir);

	//
	// add the directory to the search path
	//
	search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
	strcpy (search->filename, fs_gamedir);
	search->pack = NULL;
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	
	/*for (i=0; i<64; i++)
	{
		Com_sprintf (pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
		pak = FS_LoadPackFile (pakfile);
		if (pak)
		{
			search = Z_TagMalloc (sizeof(searchpath_t), TAGMALLOC_SEARCHPATH);
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
	}*/

	FS_LoadPaks (dir, "pak");
#ifdef _DEBUG
	FS_LoadPaks (dir, "pkz");
#endif
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char * EXPORT FS_Gamedir (void)
{
	if (fs_gamedir[0])
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

/*
============
FS_ExistsInGameDir

See if a file exists in the mod directory/paks (ignores baseq2)
============
*/
qboolean FS_ExistsInGameDir (char *filename)
{
	size_t			len;
	char			*gamedir;
	char			lowered[MAX_QPATH];
	searchpath_t	*search;
	pack_t			*pak;

	Q_strncpy (lowered, filename, sizeof(lowered)-1);
	fast_strlwr (lowered);

	gamedir = FS_Gamedir();
	len = strlen(gamedir);

	for (search = fs_searchpaths ; search ; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			packfile_t	*entry;

			//r1: optimized btree search
			pak = search->pack;

			if (strncmp (pak->filename, gamedir, len))
				continue;

			entry = rbfind (lowered, pak->rb);

			if (entry)
				return true;
		}
		else
		{
			char	netpath[MAX_OSPATH];

			if (strncmp (search->filename, gamedir, len))
				continue;

			Com_sprintf (netpath, sizeof(netpath), "%s/%s",search->filename, filename);

			if (Sys_FileLength (netpath) != -1)
				return true;
		}
	}

	return false;
}

/*
=============
FS_ExecConfig
=============
*/
void FS_ExecConfig (const char *filename)
{
	const char	*dir;
	char		name [MAX_QPATH];

	dir = Cvar_VariableString("gamedir");
	if (dir[0])
		Com_sprintf(name, sizeof(name), "%s/%s/%s", fs_basedir->string, dir, filename); 
	else
		Com_sprintf(name, sizeof(name), "%s/%s/%s", fs_basedir->string, BASEDIRNAME, filename); 
	if (Sys_FindFirst(name, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM))
		Cbuf_AddText (va ("exec %s\n", filename));
	Sys_FindClose();
}

void FS_ReloadPAKs (void)
{
	const char		*dir;
	searchpath_t	*next;

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack)
		{
			fclose (fs_searchpaths->pack->h.handle);
			//Z_Free (fs_searchpaths->pack->files);
			//RB_Purge (fs_searchpaths->pack->rb);
			rbdestroy (fs_searchpaths->pack->rb);
			Z_Free (fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Z_Free (fs_searchpaths);
		fs_searchpaths = next;
	}

	dir = Cvar_VariableString ("gamedir");

	if (dir[0] && strcmp(dir, BASEDIRNAME))
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
}

/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir (const char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strchr(dir, '/')
		|| strchr(dir, '\\') || strchr(dir, ':') )
	{
		Com_Printf ("Gamedir '%s' should be a single filename, not a path\n", LOG_GENERAL, dir);
		return;
	}

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack)
		{
			fclose (fs_searchpaths->pack->h.handle);
			//Z_Free (fs_searchpaths->pack->files);
			rbdestroy (fs_searchpaths->pack->rb);
			Z_Free (fs_searchpaths->pack);
		}
		next = fs_searchpaths->next;
		Z_Free (fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//

	FS_FlushCache();

#ifndef DEDICATED_ONLY
#ifndef NO_SERVER
	if (!dedicated->intvalue)
	{
#endif
		//Cbuf_AddText ("vid_restart\nsnd_restart\n");
		Cmd_ExecuteString ("vid_restart");
		Cmd_ExecuteString ("snd_restart");
#ifndef NO_SERVER
	}
#endif
#endif

	if (!strcmp(dir,BASEDIRNAME) || (*dir == 0))
	{
		Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, BASEDIRNAME);
		Cvar_FullSet ("gamedir", "", CVAR_SERVERINFO|CVAR_NOSET);
		Cvar_FullSet ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	}
	else
	{
		Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);
		Cvar_FullSet ("gamedir", dir, CVAR_SERVERINFO|CVAR_NOSET);
		FS_AddGameDirectory (va("%s/%s", fs_basedir->string, dir) );
	}
}


/*
================
FS_Link_f

Creates a filelink_t
================
*/
static void FS_Link_f (void)
{
	char		*to;
	filelink_t	*l, **prev;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("Purpose: Create a link from one file system path to another.\n"
					"Syntax : link <from> <to>\n"
					"Example: link ./aq2/maps ./action/maps\n", LOG_GENERAL);
		return;
	}

	//r1: validate destination. prevents people from reading outside the q2 dir
	//if rcon pass is compromised for example, rcon link ./baseq2/maps/lol.bsp /etc/passwd, download maps/lol.bsp
	to = Cmd_Argv(2);

	if (to[0])
	{
		if (strstr (to, "..") || strchr (to, '\\') || *to != '.')
		{
			Com_Printf ("Bad destination path.\n", LOG_GENERAL);
			return;
		}
	}

	// see if the link already exists
	prev = &fs_links;
	for (l=fs_links ; l ; l=l->next)
	{
		if (!strcmp (l->from, Cmd_Argv(1)))
		{
			Z_Free (l->to);
			if (!*to)
			{	// delete it
				*prev = l->next;
				Z_Free (l->from);
				Z_Free (l);
				return;
			}
			l->to = CopyString (to, TAGMALLOC_LINK);
			return;
		}
		prev = &l->next;
	}

	// create a new link
	l = Z_TagMalloc(sizeof(*l), TAGMALLOC_LINK);
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1), TAGMALLOC_LINK);
	l->fromlength = (int)strlen(l->from);
	l->to = CopyString(Cmd_Argv(2), TAGMALLOC_LINK);
}

/*
** FS_ListFiles
*/
char /*@null@*/ **FS_ListFiles( char *findname, int *numfiles, uint32 musthave, uint32 canthave )
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	s = Sys_FindFirst( findname, musthave, canthave );
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
			nfiles++;
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	if ( !nfiles )
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = malloc( sizeof( char * ) * nfiles );
	memset( list, 0, sizeof( char * ) * nfiles );

	s = Sys_FindFirst( findname, musthave, canthave );
	nfiles = 0;
	while ( s )
	{
		if ( s[strlen(s)-1] != '.' )
		{
			list[nfiles] = strdup( s );
#ifdef _WIN32
			Q_strlwr( list[nfiles] );
#endif
			nfiles++;
		}
		s = Sys_FindNext( musthave, canthave );
	}
	Sys_FindClose ();

	return list;
}

/*
** FS_Dir_f
*/
static void FS_Dir_f( void )
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if ( Cmd_Argc() != 1 )
	{
		//r1: another overflow was here!
		Q_strncpy( wildcard, Cmd_Argv( 1 ), sizeof(wildcard)-1);
	}

	while ( ( path = FS_NextPath( path ) ) != NULL )
	{
		char *tmp = findname;

		Com_sprintf( findname, sizeof(findname), "%s/%s", path, wildcard );

		while ( *tmp != 0 )
		{
			if ( *tmp == '\\' ) 
				*tmp = '/';
			tmp++;
		}
		Com_Printf( "Directory of %s\n", LOG_GENERAL, findname );
		Com_Printf( "----\n", LOG_GENERAL );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, 0, 0 ) ) != 0 )
		{
			int i;

			for ( i = 0; i < ndirs-1; i++ )
			{
				if ( strrchr( dirnames[i], '/' ) )
					Com_Printf( "%s\n", LOG_GENERAL, strrchr( dirnames[i], '/' ) + 1 );
				else
					Com_Printf( "%s\n", LOG_GENERAL, dirnames[i] );

				free( dirnames[i] );
			}
			free( dirnames );
		}
		Com_Printf( "\n", LOG_GENERAL );
	};
}

/*
============
FS_Path_f

============
*/
static void FS_Path_f (void)
{
	searchpath_t	*s;
	filelink_t		*l;

	Com_Printf ("Current search path:\n", LOG_GENERAL);
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf ("----------\n", LOG_GENERAL);
		if (s->pack)
			Com_Printf ("%s (%i files)\n", LOG_GENERAL, s->pack->filename, s->pack->numfiles);
		else
			Com_Printf ("%s\n", LOG_GENERAL, s->filename);
	}

	Com_Printf ("\nLinks:\n", LOG_GENERAL);
	for (l=fs_links ; l ; l=l->next)
		Com_Printf ("%s : %s\n", LOG_GENERAL, l->from, l->to);
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char /*@null@*/ *FS_NextPath (const char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return fs_gamedir;

	prev = fs_gamedir;
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack)
			continue;
		if (prevpath == prev)
			return s->filename;
		prev = s->filename;
	}

	return NULL;
}

/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem (void)
{
	current_filename = "unknown";

	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("link", FS_Link_f);
	Cmd_AddCommand ("dir", FS_Dir_f );

	//r1: search for a file
	Cmd_AddCommand ("whereis", FS_WhereIs_f);

	//r1: allow manual cache flushing
	Cmd_AddCommand ("fsflushcache", FS_FlushCache);

	//r1: fs stats
	Cmd_AddCommand ("fs_stats", FS_Stats_f);

	//r1: binary tree filesystem cache
	FS_InitCache ();

	//r1: init fs cache
	//FS_FlushCache ();

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get ("basedir", ".", CVAR_NOSET);
	fs_cache = Cvar_Get ("fs_cache", "7", 0);
	fs_noextern = Cvar_Get ("fs_noextern", "0", 0);

	//
	// start up with baseq2 by default
	//
	FS_AddGameDirectory (va("%s/"BASEDIRNAME, fs_basedir->string) );

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get ("game", "", CVAR_LATCH|CVAR_SERVERINFO);
	if (fs_gamedirvar->string[0])
		FS_SetGamedir (fs_gamedirvar->string);
}
