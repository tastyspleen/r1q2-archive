/*

  Client DLL functions. (New to r1q2). The client dll is a separate project that exports
  a bunch of stuff that the client engine can hook up to to manipulate local entities and
  in some way manipulate the renderer too... example:

  Server sends event X on a dead body, client parses event x in normal entity parsing.
  Event x is unhandled by the client, so it passes it on to the client DLL which could
  do one of several things such as spawn local ents, create decals in the renderer, etc.
 
  The interface between the client DLL and the engine/renderer is (attempted) to be
  controlled from this file.

  Eventually, most of the constants in the client parsing system should end up in
  the client dll, for example, all tempents should be done via the client dll. A generic
  dll will be supplied with r1q2 that maintains all the functionality of the default
  engine - ie it can be dropped into any mod dir to emulate the functions that would
  have been in the exe.

 */

//shared by the client_dll project
#ifdef CLIENT_DLL
#include "client.h"
#include "../win32/winquake.h"

HINSTANCE	cllib_library;		// Handle to client DLL 
qboolean	cllib_active = false;
clexport_t	ce;

/*
==========
CL_FreeCllib
==========
Unload the client dll and nuke anything it allocated.
*/
void CL_FreeCllib (void)
{
	if ( !FreeLibrary( cllib_library ) )
		Com_Error( ERR_FATAL, "Cllib FreeLibrary failed" );

	memset (&ce, 0, sizeof(ce));

	//nuke anything the DLL allocated
	Z_FreeTags (TAGMALLOC_CLIENT_DLL);

	cllib_library = NULL;
	cllib_active  = false;
}

/*
===========
CL_Z_Alloc
===========
Allocate memory to the client DLL via Z_TagMalloc (but without the DLL
having to specify the tag) - used to free it all when the DLL is unloaded
above.
*/
void *CL_Z_Alloc (int size)
{
	return (void *)Z_TagMalloc (size, TAGMALLOC_CLIENT_DLL);
}

/*
===========
CL_InitClientDLL
===========
Attempt to load the dll, send it the exports and grab the imports.
*/
qboolean CL_InitClientDLL (void)
{
	char name[MAX_OSPATH];
	GetClAPI_t	GetClAPI;
	climport_t	ci;

	Com_sprintf (name, sizeof(name), "%s/client_dll.dll", FS_Gamedir());

	if ( cllib_active )
	{
		ce.Shutdown();
		CL_FreeCllib ();
	}

	Com_Printf( "------ Loading client_dll.dll ------\n", LOG_CLIENT);

	if ( ( cllib_library = LoadLibrary( name ) ) == 0 )
	{
		Com_Printf( "LoadLibrary(\"%s\") failed\n", LOG_CLIENT, name );
		return false;
	}

	if ( ( GetClAPI = (GetClAPI_t)GetProcAddress( cllib_library, "GetClAPI" ) ) == 0 ) {
		Com_Printf("GetProcAddress failed on %s\n", LOG_CLIENT, name );
		return false;
	}

	ci.MSG_ReadChar = MSG_ReadChar;
	ci.MSG_ReadByte = MSG_ReadByte;
	ci.MSG_ReadShort = MSG_ReadShort;
	ci.MSG_ReadLong = MSG_ReadLong;
	ci.MSG_ReadFloat = MSG_ReadFloat;
	ci.MSG_ReadString = MSG_ReadString;
	ci.MSG_ReadStringLine = MSG_ReadStringLine;

	ci.MSG_ReadCoord= MSG_ReadCoord;
	ci.MSG_ReadPos = MSG_ReadPos;
	ci.MSG_ReadAngle = MSG_ReadAngle;
	ci.MSG_ReadAngle16 = MSG_ReadAngle16;

	ci.MSG_ReadDir = MSG_ReadDir;
	ci.MSG_ReadData = MSG_ReadData;

	ci.Cmd_AddCommand = Cmd_AddCommand;
	ci.Cmd_RemoveCommand = Cmd_RemoveCommand;

	ci.Cmd_Argc = Cmd_Argc;
	ci.Cmd_Argv = Cmd_Argv;

	ci.Cmd_ExecuteText = Cbuf_ExecuteText;

	ci.Com_Printf = Com_Printf;
	ci.Com_Error = Com_Error;

	ci.FS_LoadFile = FS_LoadFile;
	ci.FS_FreeFile = FS_FreeFile;
	ci.FS_Gamedir = FS_Gamedir;

	ci.Z_Alloc = CL_Z_Alloc;
	ci.Z_Free = Z_Free;

	ci.Cvar_Get = Cvar_Get;
	ci.Cvar_Set = Cvar_Set;
	ci.Cvar_SetValue = Cvar_SetValue;

	ce = GetClAPI( ci );

	switch (ce.api_version) {
		case 1:
			break;
		default:
			CL_FreeCllib ();
			Com_Printf (PRODUCTNAME " doesn't support your client dll version (%s)\n", LOG_CLIENT, name);
			return false;
	}

	if ( ce.Init( global_hInstance ) == -1 )
	{
		ce.Shutdown();
		CL_FreeCllib ();
		return false;
	}

	Com_Printf( "------------------------------------\n", LOG_CLIENT);
	cllib_active = true;

	return true;
}

/*
=============
CL_ClDLL_Restart_f
=============
(Re)load the client dll after switching games.
*/
void CL_ClDLL_Restart_f (void)
{
	if (!CL_InitClientDLL ()) {
		Com_Printf( "------------------------------------\n", LOG_CLIENT);
	}
}
#endif
