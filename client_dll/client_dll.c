#include "client_dll.h"

/*
	Title:	 r1q2 client DLL interface
	Author:	 R1CH (r1ch@r1ch.net)
	Version: 1.0

	Description:
	My r1q2 client provides support for a client side DLL that handles
	messages from the server such as s.event, s.effects, svc_tempentity, etc.
	By providing the client dll, each mod can have their own events, tempents,
	etc. providing a much more customisable experience without the need to edit
	the EXE. The server exports some functions to this DLL that allow you to
	do pretty much whatever you want: create new entities, spawn models, play
	sounds, execute commands, open/close files, etc. The raw interface to the
	renderer is provided allowing you to basically do anything you could do
	from the client in the exe, without the need to edit the exe. Of course,
	this dll requires r1q2 to work - something that I hope people take to using
	as there are numerous security holes, bugs, etc. in the default quake2.exe
	and in an ideal world I'd lile r1q2 to superceed idq2, and providing this
	DLL is one reason to use it instead of having 101 different exes floating
	around, each one mod-specific and perhaps not having all the needed bug
	and security fixes that others do, etc...

	A warning though, this is still under development as I continue to develop
	the Quake II engine. The client DLL api version might change at some point
	as I find the need to export more functions for example - this will make
	all older client_dll.dll files incompatible with the new exe - so be sure
	to keep up to date on what's happening...
*/


/*
==============
CLDLL_Init
==============
This is called when the client DLL is loaded from the executable.
The ci (client import) is set up at this point so you can print
status messages, check that the DLL is being called from the right
FS_GameDir(), etc. Return -1 on failure to signal the game to error
out.
*/
int CLDLL_Init ( void *hInstance)
{
	ci.Com_Printf ( "r1q2 client dll v1.0 initialized.\n"
					"This DLL is to be used with generic mods that\n"
					"do not have their own client_dll because they\n"
					"were not designed for r1q2.\n");

	return 1;
}

/*
=============
CLDLL_Shutdown
=============
This is called by the engine when it's about to unload the client DLL
from memory. Use this to ci.Z_Free() any memory you allocated, clean up
anything lying around basically. All memory allocated via ci.Z_Alloc()
will be nuked after this function exits.
*/
void CLDLL_Shutdown (void)
{
}

/*
============
GetClAPI
============
This exposes the client DLL functions to the engine and also gets the
exported engine functions passed to it. This is the only visibly
exported function in the actualy .DLL file. The remainder are done via
the pointer interfaces on the import/export structures. You should not
ever need to edit this function. Return NULL on error.
*/
CLIENT_DLL_API clexport_t GetClAPI (climport_t cimp )
{
	clexport_t	ce;

	ci = cimp;

	ce.api_version = CLIENT_DLL_API_VERSION;

	ce.Init = CLDLL_Init;
	ce.Shutdown = CLDLL_Shutdown;
	ce.CLParseTempEnt = CLParseTempEnt;

	return ce;
}

/*
============
CLParseTempEnt
============
Parse a temp ent. Return false on an unhandled tempent, the engine
will then crash the client :)
*/
qboolean CLParseTempEnt(int type)
{
	switch (type)
	{

	}

	return false;
}
