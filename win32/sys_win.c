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
// sys_win.h

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <process.h>

#ifdef USE_OPENSSL
#define OPENSSLEXPORT __cdecl
#include <openssl/sha.h>
#endif

#include "../win32/conproc.h"

int SV_CountPlayers (void);

//#define DEMO

qboolean	 global_Service = false;

HMODULE hSh32 = NULL;
FARPROC procShell_NotifyIcon = NULL;
NOTIFYICONDATA pNdata;

qboolean s_win95;

int			starttime;
int			ActiveApp;
qboolean	Minimized;

HWND		hwnd_Server;

#ifndef NO_SERVER
#ifdef USE_PYROADMIN
extern netadr_t netaddress_pyroadmin;
#endif
static HANDLE		hinput, houtput;
BOOL	oldStyleConsole = FALSE;
#endif

unsigned	sys_msg_time;
unsigned	sys_frame_time;

#ifdef USE_PYROADMIN
extern		cvar_t	*pyroadminport;
#endif

//static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];

//cvar_t	*win_priority;

sizebuf_t	console_buffer;
byte console_buff[8192] = {0};
//int console_lines = 0;

int consoleBufferPointer = 0;
byte consoleFullBuffer[16384] = {0};

//r1: service support
SERVICE_STATUS          MyServiceStatus; 
SERVICE_STATUS_HANDLE   MyServiceStatusHandle; 

//original game command line
char	cmdline[4096];
char	bname[MAX_QPATH];
/*
===============================================================================

SYSTEM IO

===============================================================================
*/

int Sys_FileLength (const char *path)
{
	WIN32_FILE_ATTRIBUTE_DATA	fileData;

	/*HANDLE	hFile;
	DWORD	length;

	hFile = CreateFile (path, FILE_READ_ATTRIBUTES, GENERIC_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return -1;

	if (GetFileSize (hFile, &length) == INVALID_FILE_SIZE)
	{
		CloseHandle (hFile);
		return -1;
	}

	CloseHandle (hFile);
	return length;*/

	if (GetFileAttributesEx (path, GetFileExInfoStandard, &fileData))
		return fileData.nFileSizeLow;
	else
		return -1;
}

void VID_Restart_f (void);
void S_Init (qboolean fullInit);
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];
	int			ret;

#ifndef DEDICATED_ONLY
	DestroyWindow (cl_hwnd);
#endif

	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsnprintf (text, sizeof(text)-1, error, argptr);
	va_end (argptr);

	if (strlen(text) < 900)
		strcat (text, "\n\nPress Retry to cause a debug breakpoint.\n");

	ret = MessageBox(NULL, text, "Quake II Fatal Error", MB_ICONEXCLAMATION | MB_ABORTRETRYIGNORE);

	if (ret == IDRETRY)
	{
		DEBUGBREAKPOINT;
	}
	else if (ret == IDIGNORE)
	{
#ifndef DEDICATED_ONLY
		VID_Restart_f ();
		S_Init (true);
#endif
		NET_Init ();
		return;
	}

	ExitProcess (0xDEAD);
}

void Sys_Quit (void)
{
	timeEndPeriod( 1 );

#ifndef DEDICATED_ONLY
	CL_Shutdown();
#endif

	Qcommon_Shutdown ();

	if (procShell_NotifyIcon)
		procShell_NotifyIcon (NIM_DELETE, &pNdata);

	if (hSh32)
		FreeLibrary (hSh32);

	if (!global_Service)
		//exit (0);
		ExitProcess (0);
}

void WinError (void)
{
	LPVOID lpMsgBuf;

	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	// Display the string.
	MessageBox( NULL, lpMsgBuf, "GetLastError", MB_OK|MB_ICONINFORMATION );

	// Free the buffer.
	LocalFree( lpMsgBuf );
}

void Sys_ServiceCtrlHandler (DWORD Opcode) 
{
    switch(Opcode) 
    { 
        case SERVICE_CONTROL_STOP: 
			// Do whatever it takes to stop here. 
			MyServiceStatus.dwWin32ExitCode = 0; 
            MyServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
            MyServiceStatus.dwCheckPoint    = 0; 
            MyServiceStatus.dwWaitHint      = 0; 
 
            SetServiceStatus (MyServiceStatusHandle, &MyServiceStatus);

			Com_Quit();
            return; 
    } 
 
    // Send current status. 
    SetServiceStatus (MyServiceStatusHandle,  &MyServiceStatus);
} 

void Sys_ServiceStart (DWORD argc, LPTSTR *argv) 
{ 
    MyServiceStatus.dwServiceType        = SERVICE_WIN32; 
    MyServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    MyServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP; 
    MyServiceStatus.dwWin32ExitCode      = 0; 
    MyServiceStatus.dwServiceSpecificExitCode = 0; 
    MyServiceStatus.dwCheckPoint         = 0; 
    MyServiceStatus.dwWaitHint           = 0; 
 
    MyServiceStatusHandle = RegisterServiceCtrlHandler( 
        argv[0], 
        (LPHANDLER_FUNCTION)Sys_ServiceCtrlHandler); 
 
    if (MyServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
        return;
 
    // Initialization complete - report running status. 
    MyServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
    MyServiceStatus.dwCheckPoint         = 0; 
    MyServiceStatus.dwWaitHint           = 0; 
 
    SetServiceStatus (MyServiceStatusHandle, &MyServiceStatus);

	WinMain (0, NULL, cmdline, 0);
 
    return; 
} 

int EXPORT main(void) 
{ 
    SERVICE_TABLE_ENTRY   DispatchTable[] = 
    { 
        { "R1Q2", (LPSERVICE_MAIN_FUNCTION)Sys_ServiceStart      }, 
        { NULL,              NULL          } 
    }; 

    if (!StartServiceCtrlDispatcher( DispatchTable)) 
		return 1;

	return 0;
} 

//================================================================

void Sys_InstallService(char *servername, char *cmdline)
{ 
	SC_HANDLE schService, schSCManager;

	char srvName[MAX_OSPATH];
	char srvDispName[MAX_OSPATH];
	char lpszBinaryPathName[2048];

	GetModuleFileName(NULL, lpszBinaryPathName, sizeof(lpszBinaryPathName));
 
	while (*cmdline != ' ') {
		cmdline++;
	}

	cmdline++;

	strcat (lpszBinaryPathName, " -service ");
	strcat (lpszBinaryPathName, cmdline);

	Com_sprintf (srvDispName, sizeof(srvDispName), "Quake II - %s", servername);
	Com_sprintf (srvName, sizeof(srvName), "R1Q2(%s)", servername);

	schSCManager = OpenSCManager( 
		NULL,                    // local machine 
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 
 
	if (schSCManager == NULL) {
		Com_Printf ("OpenSCManager FAILED. GetLastError = %d\n", LOG_GENERAL, GetLastError());
		return;
	}

    schService = CreateService( 
        schSCManager,              // SCManager database 
        srvName,				   // name of service 
        srvDispName,           // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_AUTO_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        lpszBinaryPathName,        // service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 
 
    if (schService == NULL)
	{
        Com_Printf ("CreateService FAILED. GetLastError = %d\n", LOG_GENERAL, GetLastError());
	}
	else
	{
        Com_Printf ("CreateService SUCCESS.\n", LOG_GENERAL); 
		CloseServiceHandle(schService); 
	}

	CloseServiceHandle (schSCManager);
}

void Sys_DeleteService (char *servername)
{
	SC_HANDLE schService, schSCManager;
	char srvName[MAX_OSPATH];

	snprintf (srvName, sizeof(srvName)-1, "R1Q2(%s)", servername);
	//strcpy (srvName, servername);

	schSCManager = OpenSCManager( 
		NULL,                    // local machine 
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 
 
	if (schSCManager == NULL) {
		Com_Printf ("OpenSCManager FAILED. GetLastError = %d\n", LOG_GENERAL, GetLastError());
		return;
	}

    schService = OpenService( 
        schSCManager,       // SCManager database 
        srvName,       // name of service 
        DELETE);            // only need DELETE access 

    if (schService == NULL) {
        Com_Printf ("OpenService FAILED. GetLastError = %d\n", LOG_GENERAL, GetLastError());
	} else {
		DeleteService(schService);
		CloseServiceHandle(schService); 
		Com_Printf ("DeleteService SUCCESS.\n", LOG_GENERAL); 
	}

	CloseServiceHandle (schSCManager);
}

void Sys_EnableTray (void)
{
	memset (&pNdata, 0, sizeof(pNdata));

	pNdata.cbSize = sizeof(NOTIFYICONDATA);
	pNdata.hWnd = hwnd_Server;
	pNdata.uID = 0;
	pNdata.uCallbackMessage = WM_USER + 4;
	GetWindowText (hwnd_Server, pNdata.szTip, sizeof(pNdata.szTip)-1);
	pNdata.hIcon = LoadIcon (global_hInstance, MAKEINTRESOURCE(IDI_ICON2));
	pNdata.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

	hSh32 = LoadLibrary ("shell32.dll");
	procShell_NotifyIcon = GetProcAddress (hSh32, "Shell_NotifyIcon");

	procShell_NotifyIcon (NIM_ADD, &pNdata);

	Com_Printf ("Minimize to tray enabled.\n", LOG_GENERAL);
}

void Sys_DisableTray (void)
{
	ShowWindow (hwnd_Server, SW_RESTORE);
	procShell_NotifyIcon (NIM_DELETE, &pNdata);

	if (hSh32)
		FreeLibrary (hSh32);

	procShell_NotifyIcon = NULL;

	Com_Printf ("Minimize to tray disabled.\n", LOG_GENERAL);
}

void Sys_Minimize (void)
{
	SendMessage (hwnd_Server, WM_ACTIVATE, MAKELONG(WA_INACTIVE,1), 0);
}

#ifndef NO_SERVER
void Sys_SetWindowText (char *buff)
{
	if (!global_Service)
		SetWindowText (hwnd_Server, buff);
}

void ServerWindowProcCommandExecute (void)
{
	int ret;
	char buff[1024];

	*(DWORD *)&buff = sizeof(buff)-2;

	ret = SendDlgItemMessage (hwnd_Server, IDC_COMMAND, EM_GETLINE, 1, (LPARAM)buff);
	if (!ret)
		return;
	//strcat (buff, "\n");
	buff[ret] = '\n';
	buff[ret+1] = '\0';
	Sys_ConsoleOutput (buff);
	//Cmd_ExecuteString (buff);
	Cbuf_AddText (buff);
	SendDlgItemMessage (hwnd_Server, IDC_COMMAND, WM_SETTEXT, 0, (LPARAM)"");
}

void Sys_UpdateConsoleBuffer (void)
{
	if (global_Service)
		return;

	if (console_buffer.cursize) {
		int len, buflen;

		buflen = console_buffer.cursize + 1024;
		
		if (consoleBufferPointer + buflen >= sizeof(consoleFullBuffer)) {
			int moved;
			char *p = consoleFullBuffer + buflen;
			while (*p && *p != '\n')
				p++;
			p++;
			moved = (buflen + (p - (consoleFullBuffer + buflen)));
			memmove (consoleFullBuffer, consoleFullBuffer + moved, consoleBufferPointer - moved);
			consoleBufferPointer -= moved;
			consoleFullBuffer[consoleBufferPointer] = '\0';
		}

		memcpy (consoleFullBuffer+consoleBufferPointer, console_buffer.data, console_buffer.cursize);
		consoleBufferPointer += (console_buffer.cursize - 1);

		if (!Minimized) {
			SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)consoleFullBuffer);
			len = SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0);
			SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_LINESCROLL, 0, len);
		}

		SZ_Clear (&console_buffer);
	}
}

//================================================================

LRESULT ServerWindowProcCommand(HWND hwnd, UINT message, WPARAM wParam, LONG lParam)
{
	UINT idItem = LOWORD(wParam);
	UINT wNotifyCode = HIWORD(wParam);
	//HWND hwndCtl = (HWND) lParam;

	switch (idItem) {
		case IDOK:
			switch (wNotifyCode) {
				case BN_CLICKED:
					ServerWindowProcCommandExecute();
					break;
			}
	}

	return FALSE;
}

LRESULT CALLBACK ServerWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT idItem = LOWORD(wParam);

	switch (message) {
		case WM_COMMAND:
			return ServerWindowProcCommand(hwnd, message, wParam, lParam);
		case WM_ENDSESSION:
			Cbuf_AddText ("quit exiting due to Windows shutdown.\n");
			return TRUE;
		case WM_CLOSE:
			if (!global_Service && SV_CountPlayers()) {
				int ays = MessageBox (hwnd_Server, "There are still players on the server! Really shut it down?", "WARNING!", MB_YESNO + MB_ICONEXCLAMATION);
				if (ays == IDNO)
					return TRUE;
			}
			Cbuf_AddText ("quit terminated by local request.\n");
			break;
		case WM_ACTIVATE:
			{
				int minimized = (BOOL)HIWORD(wParam);

				if (Minimized && !minimized) {
					int len;
					SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, WM_SETTEXT, 0, (LPARAM)consoleFullBuffer);
					len = SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0);
					SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_LINESCROLL, 0, len);
				}

				Minimized = minimized;
				if (procShell_NotifyIcon) {
					if (minimized && LOWORD(wParam) == WA_INACTIVE) {
						Minimized = true;
						ShowWindow (hwnd_Server, SW_HIDE);
						return FALSE;
					}
				}
				return DefWindowProc (hwnd, message, wParam, lParam);
			}
		case WM_USER + 4:
			if (lParam == WM_LBUTTONDBLCLK) {
				ShowWindow (hwnd_Server, SW_RESTORE);
				SetForegroundWindow (hwnd_Server);
				SetFocus (GetDlgItem (hwnd_Server, IDC_COMMAND));
			}
			return FALSE;
	}

	return FALSE;
}
#endif

void Sys_SetQ2Priority (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf ("usage: win_priority -2|-1|0|1|2\n", LOG_GENERAL);
		return;
	}

	//r1: let dedicated servers eat the cpu if needed
	switch (atoi(Cmd_Argv(1)))
	{
		case -2:
			SetPriorityClass (GetCurrentProcess (), IDLE_PRIORITY_CLASS);
			break;
#ifdef BELOW_NORMAL_PRIORITY_CLASS
		case -1:
			SetPriorityClass (GetCurrentProcess (), BELOW_NORMAL_PRIORITY_CLASS);
			break;
#endif
#ifdef ABOVE_NORMAL_PRIORITY_CLASS
		case 1:
			SetPriorityClass (GetCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);
			break;
#endif
		case 2:
			SetPriorityClass (GetCurrentProcess (), HIGH_PRIORITY_CLASS);
			break;
		default:
			SetPriorityClass (GetCurrentProcess (), NORMAL_PRIORITY_CLASS);
			break;
	}
}

CRITICAL_SECTION consoleCrit;

void Sys_AcquireConsoleMutex (void)
{
	EnterCriticalSection (&consoleCrit);
}

void Sys_ReleaseConsoleMutex (void)
{
	LeaveCriticalSection (&consoleCrit);
}

void Sys_InitConsoleMutex (void)
{
	InitializeCriticalSection (&consoleCrit);
}

void Sys_FreeConsoleMutex (void)
{
	DeleteCriticalSection (&consoleCrit);
}


extern	qboolean os_winxp;

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	timeBeginPeriod( 1 );

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4)
		Sys_Error ("Quake2 requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("Quake2 doesn't run on Win32s");
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
		s_win95 = true;

#ifndef DEDICATED_ONLY
	if (vinfo.dwMajorVersion >= 5)
		os_winxp = true;
#endif

#ifndef NO_SERVER

	if (dedicated->intvalue)
	{
		HICON hIcon;
		BOOL hide = FALSE;

		int i;

		for (i = 1; i < argc; i++)
		{
			if (!strcmp (argv[i], "-hideconsole"))
			{
				hide = TRUE;
			}
			else if (!strcmp (argv[i], "-oldconsole"))
			{
				if (global_Service)
					Sys_Error ("-oldconsole and service mode are incompatible");
				oldStyleConsole = TRUE;
				break;
			}
			else if (!Q_stricmp (argv[i], "-HCHILD") || !Q_stricmp (argv[i], "-HPARENT") || !Q_stricmp (argv[i], "-HFILE"))
			{
				//for gamehost compatibility
				oldStyleConsole = TRUE;
			}
		}

		if (oldStyleConsole)
		{
			if (!AllocConsole ())
				Sys_Error ("Couldn't create dedicated server console");
			hinput = GetStdHandle (STD_INPUT_HANDLE);
			houtput = GetStdHandle (STD_OUTPUT_HANDLE);
		
			// let QHOST hook in
			InitConProc (argc, argv);	
		}
		else
		{
			if (!global_Service)
			{
				hwnd_Server = CreateDialog (global_hInstance, MAKEINTRESOURCE(IDD_SERVER_GUI), NULL, (DLGPROC)ServerWindowProc);

				if (!hwnd_Server)
					Sys_Error ("Couldn't create dedicated server window. GetLastError() = %d", GetLastError());

				if (hide)
					ShowWindow (hwnd_Server, SW_HIDE);

				SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_SETREADONLY, TRUE, 0);

				SZ_Init (&console_buffer, console_buff, sizeof(console_buff));
				console_buffer.allowoverflow = true;

				//memset (consoleFullBuffer, 0, sizeof(consoleFullBuffer));

				hIcon = (HICON)LoadImage(   global_hInstance,
											MAKEINTRESOURCE(IDI_ICON2),
											IMAGE_ICON,
											GetSystemMetrics(SM_CXSMICON),
											GetSystemMetrics(SM_CYSMICON),
											0);

				//FIXME: if compiled with ICL, this icon turns into the win32 'info' icon (???)
				if(hIcon)
					SendMessage(hwnd_Server, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

				SetFocus (GetDlgItem (hwnd_Server, IDC_COMMAND));
			}
		}
	}

	Sys_InitConsoleMutex ();

	Cmd_AddCommand ("win_priority", Sys_SetQ2Priority);
#endif
}

#ifndef NO_SERVER
static char	console_text[1024];
static int	console_textlen;

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	if (!oldStyleConsole)
	{
		return NULL;
	}
	else
	{
		INPUT_RECORD	recs[1024];
		int		dummy;
		int		ch, numread, numevents;

		if (!dedicated || !dedicated->intvalue)
			return NULL;

		for ( ;; )
		{
			if (!GetNumberOfConsoleInputEvents (hinput, &numevents))
				Sys_Error ("Error getting # of console events");

			if (numevents <= 0)
				break;

			if (!ReadConsoleInput(hinput, recs, 1, &numread))
				Sys_Error ("Error reading console input");

			if (numread != 1)
				Sys_Error ("Couldn't read console input");

			if (recs[0].EventType == KEY_EVENT)
			{
				if (!recs[0].Event.KeyEvent.bKeyDown)
				{
					ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

					switch (ch)
					{
						case '\r':
							WriteFile(houtput, "\r\n", 2, &dummy, NULL);	

							if (console_textlen)
							{
								if (console_textlen >= sizeof(console_text)-1)
								{
									Com_Printf ("Sys_ConsoleInput: Line too long, discarded.\n", LOG_SERVER);
									return NULL;
								}
								console_text[console_textlen] = 0;
								console_textlen = 0;
								return console_text;
							}
							break;

						case '\b':
							if (console_textlen)
							{
								console_textlen--;
								WriteFile(houtput, "\b \b", 3, &dummy, NULL);	
							}
							break;

						default:
							if (ch >= ' ')
							{
								if (console_textlen < sizeof(console_text)-2)
								{
									WriteFile(houtput, &ch, 1, &dummy, NULL);	
									console_text[console_textlen] = ch;
									console_textlen++;
								}
							}

							break;

					}
				}
			}
		}
		return NULL;
	}
}

void Sys_ConsoleOutputOld (const char *string)
{
	int		dummy;
	char	text[256];

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, string, (DWORD)strlen(string), &dummy, NULL);

	if (console_textlen)
		WriteFile(houtput, console_text, console_textlen, &dummy, NULL);
}

/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (const char *string)
{
	char text[2048];
	const char *p;
	char *s;
	//int n = 0;

	if (!dedicated->intvalue)
		return;

	if (oldStyleConsole)
	{
		Sys_ConsoleOutputOld (string);
		return;
	}

	//r1: no output for services, non dedicated and not before buffer is initialized.
	if (global_Service || !console_buffer.maxsize)
		return;

	Sys_AcquireConsoleMutex();

#ifdef USE_PYROADMIN
	if (pyroadminport->intvalue)
	{
		int len;
		char buff[1152];
		len = Com_sprintf (buff, sizeof(buff), "line\n%s", string);
		Netchan_OutOfBand (NS_SERVER, &netaddress_pyroadmin, len, (byte *)buff);
	}
#endif

	p = string;
	s = text;

	while (*p)
	{
		if (*p == '\n') {
			*s++ = '\r';
			//console_lines++;
		}

		//r1: strip high bits here
		*s = (*p) & 127;

		if (*s >= 32 || *s == '\n' || *s == '\t')
			s++;

		p++;

		if ((s - text) >= sizeof(text)-2) {
			*s++ = '\n';
			break;
		}
	}
	*s = '\0';

	//MessageBox (NULL, text, "hi", MB_OK);
	//if (console_buffer.cursize + strlen(text)+2 > console_buffer.maxsize)
	SZ_Print (&console_buffer, text);

	Sys_UpdateConsoleBuffer();
	Sys_ReleaseConsoleMutex();
}

#endif

void Sys_Sleep (int msec)
{
	Sleep (msec);
}

/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
	if (g_pKeyboard)
	{
		IN_ReadKeyboard ();
	}
	else
	{
		MSG        msg;

		//Com_Printf ("sske\n", LOG_GENERAL);

		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
		{
			//if (GetMessage (&msg, NULL, 0, 0) == -1)
			//	Com_Quit ();
			sys_msg_time = msg.time;
      		TranslateMessage (&msg);
      		DispatchMessage (&msg);
		}
	}

	// grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 )
		{
			if ( ( cliptext = GlobalLock( hClipboardData ) ) != 0 ) 
			{
				data = malloc( GlobalSize( hClipboardData ) + 1 );
				strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
==============================================================================

 WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
#ifndef DEDICATED_ONLY
	ShowWindow ( cl_hwnd, SW_RESTORE);
	SetForegroundWindow ( cl_hwnd );
#endif
}

/*
========================================================================

GAME DLL

========================================================================
*/
#ifndef NO_SERVER
static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms, int baseq2DLL)
{
	void	*(IMPORT *GetGameAPI) (void *);
	char	newname[MAX_OSPATH];
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
	FILE	*newExists;

#if defined _M_IX86
	const char *gamename = "gamex86.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined _M_ALPHA
	const char *gamename = "gameaxp.dll";

#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#elif defined _WIN64

	const char *gamename = "gamex64.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#else
#error Don't know what kind of dynamic objects to use for this architecture.
#endif

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof(cwd));
	Com_sprintf (newname, sizeof(newname), "%s/%s/%s.new", cwd, debugdir, gamename);
	Com_sprintf (name, sizeof(name), "%s/%s/%s", cwd, debugdir, gamename);
	newExists = fopen (newname, "rb");
	if (newExists)
	{
		Com_DPrintf ("%s.new found, moving to %s...\n", gamename, gamename);
		fclose (newExists);
		unlink (name);
		rename (newname, name);
	}
	game_library = LoadLibrary ( name );
	if (game_library)
	{
		Com_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
#ifdef _DEBUG
		// check the current directory for other development purposes
		Com_sprintf (name, sizeof(name), "%s/%s", cwd, gamename);
		Com_sprintf (newname, sizeof(newname), "%s/%s.new", cwd, gamename);
		newExists = fopen (newname, "rb");
		if (newExists)
		{
			Com_DPrintf ("%s.new found, moving to %s...\n", gamename, gamename);
			fclose (newExists);
			unlink (name);
			rename (newname, name);
		}
		game_library = LoadLibrary ( name );
		if (game_library)
		{
			Com_DPrintf ("LoadLibrary (%s)\n", name);
		}
		else
#endif
		{
			// now run through the search paths
			if (baseq2DLL)
			{
				Com_sprintf (name, sizeof(name), "./" BASEDIRNAME "/%s", gamename);
				game_library = LoadLibrary (name);
			}
			else
			{
				path = NULL;
				for (;;)
				{
					path = FS_NextPath (path);
					if (!path)
						return NULL;		// couldn't find one anywhere
					Com_sprintf (name, sizeof(name), "%s/%s", path, gamename);
					Com_sprintf (newname, sizeof(newname), "%s/%s.new", path, gamename);
					newExists = fopen (newname, "rb");
					if (newExists)
					{
						Com_DPrintf ("%s.new found, moving to %s...\n", gamename, gamename);
						fclose (newExists);
						unlink (name);
						rename (newname, name);
					}
					game_library = LoadLibrary (name);
					if (game_library)
					{
						Com_DPrintf ("LoadLibrary (%s)\n",name);
						break;
					}
				}
			}
		}
	}

	if (!game_library)
		return NULL;

	GetGameAPI = (void *(IMPORT *)(void *))GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI (parms);
}
#endif

//=======================================================================


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	char *p;

	argc = 1;
	argv[0] = "exe";

	GetModuleFileName (NULL, bname, sizeof(bname)-1);
	p = strrchr (bname, '\\');
	if (p)
		binary_name = p + 1;
	else
		binary_name = bname;

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
		}
	}
}

/*#ifndef NO_SERVER
void QuakeMain (void)
{
	int				time, oldtime, newtime;

	oldtime = Sys_Milliseconds ();

	_controlfp( _PC_24, _MCW_PC );

	for (;;)
	{
		if (Minimized || dedicated->intvalue)
			Sleep (1);

		do
		{
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);

		Qcommon_Frame (time);

		oldtime = newtime;
	}
}
#endif*/

void FixWorkingDirectory (void)
{
	char *p;
	char curDir[MAX_OSPATH];

	GetModuleFileName (NULL, curDir, sizeof(curDir)-1);
	p = strrchr (curDir, '\\');
	*p = 0;

	SetCurrentDirectory (curDir);
}

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;

//#define FLOAT2INTCAST(f)(*((int *)(&f)))
//#define FLOAT_GT_ZERO(f) (FLOAT2INTCAST(f) > 0)

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifndef NO_SERVER
//	unsigned int	handle;
#endif
    MSG				msg;
	int				time, oldtime, newtime;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	if (hInstance)
		Q_strncpy (cmdline, lpCmdLine, sizeof(cmdline)-1);

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	//r1ch: always change to our directory (ugh)
	FixWorkingDirectory ();

	//hInstance is empty when we are back here with service code
	if (hInstance && argc > 1)
	{
		if (!strcmp(argv[1], "-service"))
		{
			global_Service = true;
			return main ();
		}
	}

	Qcommon_Init (argc, argv);

	_controlfp( _PC_24, _MCW_PC );

/*#ifndef NO_SERVER

	if (dedicated->intvalue)
	{
		//_beginthreadex (NULL, 0, (unsigned int (__stdcall *)(void *))QuakeMain, NULL, 0, &handle);
		CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)QuakeMain, NULL, 0, &handle);

		while (GetMessage (&msg, NULL, 0, 0))
		{
			sys_msg_time = msg.time;

			if (!IsDialogMessage(hwnd_Server, &msg)) {
				TranslateMessage (&msg);
   				DispatchMessage (&msg);
			}
		}

		Com_Quit ();
	} else {
#endif
#ifndef DEDICATED_ONLY*/

		oldtime = Sys_Milliseconds ();
		/* main window message loop */
		for (;;)
		{
			// if at a full screen console, don't update unless needed
			if (Minimized
	/*#ifndef NO_SERVER			
				|| (dedicated->intvalue)
	#endif*/
			)
			{
				Sleep (1);
			}

			while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
			{
				//if (GetMessage (&msg, NULL, 0, 0) == -1)
				//	Com_Quit ();
				sys_msg_time = msg.time;

	#ifndef NO_SERVER
				if (!hwnd_Server || !IsDialogMessage(hwnd_Server, &msg))
				{
	#endif
					TranslateMessage (&msg);
   					DispatchMessage (&msg);
	#ifndef NO_SERVER
				}
	#endif
			}

			//for (;;)
			//{
			//	if (Minimized)
			//		Sleep (1);

				do
				{
					newtime = Sys_Milliseconds ();
					time = newtime - oldtime;
				} while (time < 1);

				Qcommon_Frame (time);

				oldtime = newtime;
			//}
		}
/*#endif
#ifndef NO_SERVER
	}
#endif*/

	return 0;
}
