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

#define WIN32_LEAN_AND_MEAN
#include "../qcommon/qcommon.h"
#include "winquake.h"
#include <mmsystem.h>
#include <direct.h>
#include <io.h>

//===============================================================================

int		hunkcount;


int		hunkmaxsize;
int		cursize;

//#define	VIRTUAL_ALLOC 1
//#define CREATE_HEAP 1

#if CREATE_HEAP
HANDLE	membase;
#else
byte	*membase;
#endif

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	cursize = 0;
	hunkmaxsize = maxsize;
#if VIRTUAL_ALLOC
	membase = VirtualAlloc (NULL, maxsize, MEM_RESERVE, PAGE_NOACCESS);
#elif CREATE_HEAP
	{
		ULONG lfh = 2;
		membase = HeapCreate (HEAP_NO_SERIALIZE | HEAP_GENERATE_EXCEPTIONS, 0, maxsize);
		HeapSetInformation (membase, HeapCompatibilityInformation, &lfh, sizeof(lfh));
	}
#else
	membase = malloc (maxsize);
	memset (membase, 0, maxsize);
#endif
	if (!membase)
		Sys_Error ("VirtualAlloc reserve failed");
	return (void *)membase;
}

void *Hunk_Alloc (int size)
{
#if VIRTUAL_ALLOC || CREATE_HEAP
	void	*buf;
#endif

	// round to cacheline
	size = (size+31)&~31;

#if CREATE_HEAP
	buf = HeapAlloc (membase, 0, size);
	if (!buf)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("HeapAlloc failed.\n%s", buf);
	}
	cursize += size;
	if (cursize > hunkmaxsize)
		Sys_Error ("HeapAlloc overflow");
	return buf;
#else

#if VIRTUAL_ALLOC
	// commit pages as needed
//	buf = VirtualAlloc (membase+cursize, size, MEM_COMMIT, PAGE_READWRITE);
	buf = VirtualAlloc (membase, cursize+size, MEM_COMMIT, PAGE_READWRITE);
	if (!buf)
	{
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Sys_Error ("VirtualAlloc commit failed.\n%s", buf);
	}
#endif
	cursize += size;
	if (cursize > hunkmaxsize)
		Sys_Error ("Hunk_Alloc overflow");

	return (void *)(membase+cursize-size);
#endif
}

int Hunk_End (void)
{

	// free the remaining unused virtual memory
#if 0
	void	*buf;

	// write protect it
	buf = VirtualAlloc (membase, cursize, MEM_COMMIT, PAGE_READONLY);
	if (!buf)
		Sys_Error ("VirtualAlloc commit failed");
#endif

	hunkcount++;
//Com_Printf ("hunkcount: %i\n", hunkcount);
	return cursize;
}

void Hunk_Free (void *base)
{
	if ( base )
#if VIRTUAL_ALLOC
		VirtualFree (base, 0, MEM_RELEASE);
#elif CREATE_HEAP
		HeapDestroy (membase);
#else
		free (base);
#endif

	hunkcount--;
}

//===============================================================================


/*
================
Sys_Milliseconds
================
*/
int	curtime;
//int oldcurtime;
#ifndef BUILDING_REF_GL
int Sys_Milliseconds (void)
{
	static int		base;
	static qboolean	initialized = false;

	if (!initialized)
	{	// let base retain 16 bits of effectively random data
		base = timeGetTime() & 0xffff0000;
		initialized = true;
//		oldcurtime = 0;
	}
	
	curtime = timeGetTime() - base;

	/*if (curtime < oldcurtime)
		Com_Printf ("Sys_Milliseconds: Uh oh! Timer wrapped!\n");

	oldcurtime = curtime;*/

	return curtime;
}
#endif

void Sys_Mkdir (char *path)
{
	_mkdir (path);
}

//============================================

//r1: changed to use Win32 API rather than libc

char	findbase[MAX_OSPATH];
char	findpath[MAX_OSPATH];
HANDLE	findhandle;

static qboolean CompareAttributes( DWORD found, unsigned musthave, unsigned canthave )
{
	if ( ( found & FILE_ATTRIBUTE_READONLY ) && ( canthave & SFF_RDONLY ) )
		return false;
	if ( ( found & FILE_ATTRIBUTE_HIDDEN ) && ( canthave & SFF_HIDDEN ) )
		return false;
	if ( ( found & FILE_ATTRIBUTE_SYSTEM ) && ( canthave & SFF_SYSTEM ) )
		return false;
	if ( ( found & FILE_ATTRIBUTE_DIRECTORY ) && ( canthave & SFF_SUBDIR ) )
		return false;
	if ( ( found & FILE_ATTRIBUTE_ARCHIVE ) && ( canthave & SFF_ARCH ) )
		return false;

	if ( ( musthave & SFF_RDONLY ) && !( found & FILE_ATTRIBUTE_READONLY ) )
		return false;
	if ( ( musthave & SFF_HIDDEN ) && !( found & FILE_ATTRIBUTE_HIDDEN ) )
		return false;
	if ( ( musthave & SFF_SYSTEM ) && !( found & FILE_ATTRIBUTE_SYSTEM ) )
		return false;
	if ( ( musthave & SFF_SUBDIR ) && !( found & FILE_ATTRIBUTE_DIRECTORY ) )
		return false;
	if ( ( musthave & SFF_ARCH ) && !( found & FILE_ATTRIBUTE_ARCHIVE ) )
		return false;

	return true;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave )
{
	WIN32_FIND_DATA	findinfo;

	if (findhandle)
		Sys_Error ("Sys_BeginFind without close");

	COM_FilePath (path, findbase);
	findhandle = FindFirstFile (path, &findinfo);

	if (findhandle == INVALID_HANDLE_VALUE)
		return NULL;

	if (!CompareAttributes( findinfo.dwFileAttributes, musthave, canthave ) )
		return NULL;

	Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.cFileName);
	return findpath;
}

char *Sys_FindNext ( unsigned musthave, unsigned canthave )
{
	WIN32_FIND_DATA	findinfo;

	if (findhandle == INVALID_HANDLE_VALUE)
		return NULL;

	if (!FindNextFile (findhandle, &findinfo))
		return NULL;

	if (!CompareAttributes( findinfo.dwFileAttributes, musthave, canthave ) )
		return NULL;

	Com_sprintf (findpath, sizeof(findpath), "%s/%s", findbase, findinfo.cFileName);
	return findpath;
}

void Sys_FindClose (void)
{
	if (findhandle != INVALID_HANDLE_VALUE)
		FindClose (findhandle);

	findhandle = 0;
}


//============================================

