#ifdef CLIENT_DLL_EXPORTS
#define CLIENT_DLL_API __declspec(dllexport)
#else
#define CLIENT_DLL_API __declspec(dllimport)
#endif

#include "../client/client.h"
#include "../win32/winquake.h"

climport_t ci;

//client DLL api version. this should be bumped up if the exports or imports in client.h
//have been changed to keep the engine and client_dll synced.
#define CLIENT_DLL_API_VERSION 1

//prototypes of the functions in the client_dll.c
qboolean CLParseTempEnt(int type);

