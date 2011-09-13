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

#define _WINNT_VER 0x5

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
#include <dbghelp.h>
#include <Richedit.h>

#ifdef USE_OPENSSL
#define OPENSSLEXPORT __cdecl
#include <openssl/sha.h>
#endif

#ifdef USE_CURL
#define CURL_STATICLIB
#define CURL_HIDDEN_SYMBOLS
#define CURL_EXTERN_SYMBOL
#define CURL_CALLING_CONVENTION __cdecl
#include <curl/curl.h>
#endif

#include "../win32/conproc.h"

int SV_CountPlayers (void);

//#define DEMO

#ifdef DEDICATED_ONLY
qboolean	 global_Service = false;
#endif

HMODULE hSh32 = NULL;
FARPROC procShell_NotifyIcon = NULL;
NOTIFYICONDATA pNdata;

cvar_t	*win_priority;
//cvar_t	*win_disablewinkey;

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

uint32	sys_msg_time;
uint32	sys_frame_time;

#ifdef USE_PYROADMIN
extern		cvar_t	*pyroadminport;
#endif

cvar_t		*win_disableexceptionhandler = &uninitialized_cvar;
cvar_t		*win_silentexceptionhandler = &uninitialized_cvar;
cvar_t		*sys_fpu_bits = &uninitialized_cvar;

//static HANDLE		qwclsemaphore;

#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];

//cvar_t	*win_priority;

sizebuf_t	console_buffer;
byte		console_buff[16384];

//r1: service support
#ifdef DEDICATED_ONLY
SERVICE_STATUS          MyServiceStatus; 
SERVICE_STATUS_HANDLE   MyServiceStatusHandle; 
#endif

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

	if (GetFileAttributesEx (path, GetFileExInfoStandard, &fileData))
		return (int)fileData.nFileSizeLow;
	else
		return -1;
}

void VID_Restart_f (void);
void S_Init (int fullInit);
NORETURN void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];
	int			ret;

#ifndef DEDICATED_ONLY
	if (cl_hwnd && IsWindow (cl_hwnd))
		DestroyWindow (cl_hwnd);
#endif

	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsnprintf (text, sizeof(text)-1, error, argptr);
	va_end (argptr);

	text[sizeof(text)-1] = 0;

	if (strlen(text) < 900)
		strcat (text, "\r\n\r\nWould you like to debug? (DEVELOPERS ONLY!)\n");

rebox:;

	ret = MessageBox(NULL, text, "Quake II Fatal Error", MB_ICONEXCLAMATION | MB_YESNO);

	if (ret == IDYES)
	{
		ret = MessageBox(NULL, "Please attach your debugger now to prevent the built in exception handler from catching the breakpoint. When ready, press Yes to cause a breakpoint or No to cancel.", "Quake II Fatal Error", MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2);
		if (ret == IDYES)
		{
#ifndef _DEBUG
			if (!IsDebuggerPresent ())
			{
				ExitProcess (0x1d107);
			}
#endif
			Sys_DebugBreak ();
		}
		else
		{
			goto rebox;
		}
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

    // Cleanup before shutdown
    //UnhookWindowsHookEx( g_hKeyboardHook );

	if (procShell_NotifyIcon)
		procShell_NotifyIcon (NIM_DELETE, &pNdata);

	if (hSh32)
		FreeLibrary (hSh32);

#ifdef DEDICATED_ONLY
	if (!global_Service)
#endif
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

#ifdef DEDICATED_ONLY

void EXPORT Sys_ServiceCtrlHandler (DWORD Opcode) 
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

void EXPORT Sys_ServiceStart (DWORD argc, LPTSTR *argv) 
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
#endif

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
#ifdef DEDICATED_ONLY
	if (!global_Service)
#endif
		SetWindowText (hwnd_Server, buff);
}

void ServerWindowProcCommandExecute (void)
{
	int			ret;
	char		buff[1024];

	*(DWORD *)&buff = sizeof(buff)-2;

	ret = (int)SendDlgItemMessage (hwnd_Server, IDC_COMMAND, EM_GETLINE, 1, (LPARAM)buff);
	if (!ret)
		return;

	buff[ret] = '\n';
	buff[ret+1] = '\0';
	Sys_ConsoleOutput (buff);
	//Cmd_ExecuteString (buff);
	Cbuf_AddText (buff);
	SendDlgItemMessage (hwnd_Server, IDC_COMMAND, WM_SETTEXT, 0, (LPARAM)"");
}

void Sys_UpdateConsoleBuffer (void)
{
#ifdef DEDICATED_ONLY
	if (global_Service)
		return;
#endif

	if (console_buffer.cursize)
	{
		int len;

		/*buflen = console_buffer.cursize + 1024;
		
		if (consoleBufferPointer + buflen >= sizeof(consoleFullBuffer))
		{
			int		moved;
			char	*p = consoleFullBuffer + buflen;
			char	*q;

			while (p[0] && p[0] != '\n')
				p++;
			p++;
			q = (consoleFullBuffer + buflen);
			moved = (buflen + (int)(p - q));
			memmove (consoleFullBuffer, consoleFullBuffer + moved, consoleBufferPointer - moved);
			consoleBufferPointer -= moved;
			consoleFullBuffer[consoleBufferPointer] = '\0';
		}

		memcpy (consoleFullBuffer+consoleBufferPointer, console_buffer.data, console_buffer.cursize);
		consoleBufferPointer += (console_buffer.cursize - 1);*/

		SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
		SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_REPLACESEL, 0, (LPARAM)console_buffer.data);

		while ((len = (int)SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_GETLINECOUNT, 0, 0)) > 1000)
		{
			int	line_length;
			line_length = (int)SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_LINELENGTH, 0, 0);

			SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_SETSEL, 0, line_length + 1);
			SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_REPLACESEL, 0, (LPARAM)"");
		}

		SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
		SendDlgItemMessage (hwnd_Server, IDC_CONSOLE, WM_VSCROLL, SB_BOTTOM, 0);

		SZ_Clear (&console_buffer);
	}
}

//================================================================

LRESULT ServerWindowProcCommand(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
#ifdef DEDICATED_ONLY
			if (!global_Service)
#endif
			{
				if (SV_CountPlayers()) {
					int ays = MessageBox (hwnd_Server, "There are still players on the server! Really shut it down?", "WARNING!", MB_YESNO + MB_ICONEXCLAMATION);
					if (ays == IDNO)
						return TRUE;
				}
			}
			Cbuf_AddText ("quit terminated by local request.\n");
			break;
		case WM_ACTIVATE:
			{
				int minimized = (BOOL)HIWORD(wParam);

				Minimized = minimized;
				if (procShell_NotifyIcon)
				{
					if (minimized && LOWORD(wParam) == WA_INACTIVE)
					{
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

void _priority_changed (cvar_t *cvar, char *ov, char *nv)
{
	//r1: let dedicated servers eat the cpu if needed
	switch (cvar->intvalue)
	{
		case -2:
			SetPriorityClass (GetCurrentProcess (), IDLE_PRIORITY_CLASS);
			break;
#ifdef BELOW_NORMAL_PRIORITY_CLASS
		case -1:
			SetPriorityClass (GetCurrentProcess (), BELOW_NORMAL_PRIORITY_CLASS);
			break;
#endif
		case 0:
			SetPriorityClass (GetCurrentProcess (), NORMAL_PRIORITY_CLASS);
			break;
#ifdef ABOVE_NORMAL_PRIORITY_CLASS
		case 1:
			SetPriorityClass (GetCurrentProcess (), ABOVE_NORMAL_PRIORITY_CLASS);
			break;
#endif
		case 2:
			SetPriorityClass (GetCurrentProcess (), HIGH_PRIORITY_CLASS);
			break;
		default:
			Com_Printf ("Unknown priority class %d.\n", LOG_GENERAL, cvar->intvalue);
			Cvar_Set (cvar->name, ov);
			break;
	}
}

//CRITICAL_SECTION consoleCrit;

void Sys_AcquireConsoleMutex (void)
{
	//EnterCriticalSection (&consoleCrit);
}

void Sys_ReleaseConsoleMutex (void)
{
	//LeaveCriticalSection (&consoleCrit);
}

void Sys_InitConsoleMutex (void)
{
	//InitializeCriticalSection (&consoleCrit);
}

void Sys_FreeConsoleMutex (void)
{
	//DeleteCriticalSection (&consoleCrit);
}

qboolean os_winxp = false;

/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	timeBeginPeriod( 1 );

	//initializes base time
	Sys_Milliseconds ();

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
#ifdef DEDICATED_ONLY
				if (global_Service)
					Sys_Error ("-oldconsole and service mode are incompatible");
#endif
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
#ifdef DEDICATED_ONLY
			if (!global_Service)
#endif
			{
				if (!LoadLibrary ("Riched20"))
					Sys_Error ("Couldn't load RICHED20.DLL. GetLastError() = %d", GetLastError());

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
	else
	{
		// Initialization
	}

	Sys_InitConsoleMutex ();

#ifndef _DEBUG
	sys_fpu_bits = Cvar_Get ("sys_fpu_bits", "2", CVAR_NOSET);
#else
	sys_fpu_bits = Cvar_Get ("sys_fpu_bits", "2", 0);
#endif

	//Cmd_AddCommand ("win_priority", Sys_SetQ2Priority);
	win_priority = Cvar_Get ("win_priority", "0", 0);
	win_priority->changed = _priority_changed;
	win_priority->changed (win_priority, "0", win_priority->string);
	win_disableexceptionhandler = Cvar_Get ("win_disableexceptionhandler", "0", 0);
	win_silentexceptionhandler = Cvar_Get ("win_silentexceptionhandler", "0", 0);
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
	Sys_UpdateConsoleBuffer ();

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
	int			dummy;
	char		text[256];
	char		cleanstring[2048];
	const char	*p;
	char		*s;

	p = string;
	s = cleanstring;

	while (p[0])
	{
		if (p[0] == '\n')
		{
			*s++ = '\r';
		}

		//r1: strip high bits here
		*s = (p[0]) & 127;

		if (s[0] >= 32 || s[0] == '\n' || s[0] == '\t')
			s++;

		p++;

		if ((s - cleanstring) >= sizeof(cleanstring)-2)
		{
			*s++ = '\n';
			break;
		}
	}
	s[0] = '\0';

	if (console_textlen)
	{
		text[0] = '\r';
		memset(&text[1], ' ', console_textlen);
		text[console_textlen+1] = '\r';
		text[console_textlen+2] = 0;
		WriteFile(houtput, text, console_textlen+2, &dummy, NULL);
	}

	WriteFile(houtput, cleanstring, (DWORD)strlen(cleanstring), &dummy, NULL);

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

#ifdef DEDICATED_ONLY
	if (global_Service)
		return;
#endif

	//r1: no output for services, non dedicated and not before buffer is initialized.
	if (!console_buffer.maxsize)
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

	while (p[0])
	{
		if (p[0] == '\n')
		{
			*s++ = '\r';
		}

		//r1: strip high bits here
		*s = (p[0]) & 127;

		if (s[0] >= 32 || s[0] == '\n' || s[0] == '\t')
			s++;

		p++;

		if ((s - text) >= sizeof(text)-2)
		{
			*s++ = '\n';
			break;
		}
	}
	s[0] = '\0';

	//MessageBox (NULL, text, "hi", MB_OK);
	//if (console_buffer.cursize + strlen(text)+2 > console_buffer.maxsize)
	SZ_Print (&console_buffer, text);
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
#ifndef DEDICATED_ONLY
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

		//while (GetInputState())
		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
		{
			//if (GetMessage (&msg, NULL, 0, 0) == -1)
				//Com_Quit ();
			sys_msg_time = msg.time;
      		TranslateMessage (&msg);
      		DispatchMessage (&msg);
		}
	}

	// grab frame time 
	sys_frame_time = timeGetTime();	// FIXME: should this be at start?
}
#endif

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
		Sys_Error ("FreeLibrary failed for game library (%d)", GetLastError());
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
	const char gamename[] = "gamex86.dll";

#ifdef NDEBUG
	const char debugdir[] = "release";
#else
	const char debugdir[] = "debug";
#endif

#elif defined _M_ALPHA
	const char gamename[] = "gameaxp.dll";

#ifdef NDEBUG
	const char debugdir[] = "releaseaxp";
#else
	const char debugdir[] = "debugaxp";
#endif

#elif defined _WIN64

	const char gamename[] = "gamex86_64.dll";

#ifdef NDEBUG
	const char debugdir[] = "release";
#else
	const char debugdir[] = "debug";
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
		remove (name);
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
			remove (name);
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
						remove (name);
						rename (newname, name);
					}
					game_library = LoadLibrary (name);
					if (game_library)
					{
						Com_DPrintf ("LoadLibrary (%s)\n",name);
						break;
					}
					else
						Com_DPrintf ("LoadLibrary (%s) = %d\n", name, GetLastError());
				}
			}
		}
	}

	if (!game_library)
		return NULL;

	//if (!Sys_CheckFPUStatus())
	//	Com_Printf ("\2WARNING: The FPU control word was changed after loading %s!\n", LOG_GENERAL, name);

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
	int		i;
	char	*p;
	char	curDir[MAX_PATH];

	GetModuleFileName (NULL, curDir, sizeof(curDir)-1);

	p = strrchr (curDir, '\\');
	p[0] = 0;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp (argv[i], "-nopathcheck"))
			goto skipPathCheck;

		if (!strcmp (argv[i], "-nocwdcheck"))
			return;
	}

	if (strlen(curDir) > (MAX_OSPATH - MAX_QPATH))
		Sys_Error ("Current path is too long. Please move your Quake II installation to a shorter path.");

skipPathCheck:

	SetCurrentDirectory (curDir);
}

#ifdef ANTICHEAT
#ifndef DEDICATED_ONLY
/*
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
 
BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;
	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle("KERNEL32"),"IsWow64Process");
 
    if (NULL != fnIsWow64Process)
    {
        fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
    }
    return bIsWow64;
}*/

typedef struct
{
	void (*Check) (void);
} anticheat_export_t;

anticheat_export_t *anticheat;

typedef VOID * (*FNINIT) (VOID);

int Sys_GetAntiCheatAPI (void)
{
	qboolean			updated = false;
	HMODULE				hAC;
	static FNINIT		init = NULL;

	//windows version check
	if (s_win95)
	{
		Com_Printf ("ERROR: Anticheat requires Windows 2000/XP/2003/Vista.\n", LOG_GENERAL);
		return 0;
	}

	/*if (IsWow64())
	{
		Com_Printf ("ERROR: Anticheat is currently incompatible with 64 bit Windows.\n", LOG_GENERAL);
		return 0;
	}*/

	//already loaded, just reinit
	if (anticheat)
	{
		anticheat = (anticheat_export_t *)init ();
		if (!anticheat)
			return 0;
		return 1;
	}

reInit:

#if defined(_DEBUG)
	hAC = LoadLibrary ("anticheatd");
#else
	hAC = LoadLibrary ("anticheat");
#endif
	if (!hAC)
		return 0;

	//this should never fail unless the anticheat.dll is bad
	if (!init)
	{
		init = (FNINIT)GetProcAddress (hAC, "Initialize");
		if (!init)
			Sys_Error ("Couldn't GetProcAddress Initialize on anticheat.dll!\r\n\r\nPlease check you are using a valid anticheat.dll from http://antiche.at/");
	}

	anticheat = (anticheat_export_t *)init ();

	if (!updated && !anticheat)
	{
		updated = true;
		FreeLibrary (hAC);
		hAC = NULL;
		init = NULL;
		goto reInit;
	}

	if (!anticheat)
		return 0;

	//if (!Sys_CheckFPUStatus ())
	//	Com_Printf ("\2WARNING: The FPU control word has changed after loading anticheat!\n", LOG_GENERAL);

	return 1;
}
#endif
#endif

typedef BOOL (WINAPI *ENUMERATELOADEDMODULES64) (HANDLE hProcess, PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback, PVOID UserContext);
typedef DWORD (WINAPI *SYMSETOPTIONS) (DWORD SymOptions);
typedef BOOL (WINAPI *SYMINITIALIZE) (HANDLE hProcess, PSTR UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (WINAPI *SYMCLEANUP) (HANDLE hProcess);
typedef BOOL (WINAPI *STACKWALK64) (
								DWORD MachineType,
								HANDLE hProcess,
								HANDLE hThread,
								LPSTACKFRAME64 StackFrame,
								PVOID ContextRecord,
								PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
								PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
								PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
								PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
								);

typedef PVOID	(WINAPI *SYMFUNCTIONTABLEACCESS64) (HANDLE hProcess, DWORD64 AddrBase);
typedef DWORD64 (WINAPI *SYMGETMODULEBASE64) (HANDLE hProcess, DWORD64 dwAddr);
typedef BOOL	(WINAPI *SYMFROMADDR) (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);
typedef BOOL	(WINAPI *SYMGETMODULEINFO64) (HANDLE hProcess, DWORD64 dwAddr, PIMAGEHLP_MODULE64 ModuleInfo);

typedef DWORD64 (WINAPI *SYMLOADMODULE64) (HANDLE hProcess, HANDLE hFile, PSTR ImageName, PSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP) (
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

typedef HINSTANCE (WINAPI *SHELLEXECUTEA) (HWND hwnd, LPCTSTR lpOperation, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd);

SYMGETMODULEINFO64	fnSymGetModuleInfo64;
SYMLOADMODULE64		fnSymLoadModule64;

typedef BOOL (WINAPI *VERQUERYVALUE) (const LPVOID pBlock, LPTSTR lpSubBlock, PUINT lplpBuffer, PUINT puLen);
typedef DWORD (WINAPI *GETFILEVERSIONINFOSIZE) (LPTSTR lptstrFilename, LPDWORD lpdwHandle);
typedef BOOL (WINAPI *GETFILEVERSIONINFO) (LPTSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);

VERQUERYVALUE			fnVerQueryValue;
GETFILEVERSIONINFOSIZE	fnGetFileVersionInfoSize;
GETFILEVERSIONINFO		fnGetFileVersionInfo;
BOOL					versionedGL = FALSE;

BOOL CALLBACK EnumerateLoadedModulesProcDump (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	VS_FIXEDFILEINFO *fileVersion;
	BYTE	*verInfo;
	DWORD	dummy, len;
	FILE	*fhReport = (FILE *)UserContext;
	CHAR	verString[32];
	CHAR	lowered[MAX_PATH];

	Q_strncpy (lowered, ModuleName, sizeof(lowered)-1);
	strlwr (lowered);

	if (fnGetFileVersionInfo && fnVerQueryValue && fnGetFileVersionInfoSize)
	{
		if (len = (fnGetFileVersionInfoSize (ModuleName, &dummy)))
		{
			verInfo = LocalAlloc (LPTR, len);
			if (fnGetFileVersionInfo (ModuleName, dummy, len, verInfo))
			{
				if (fnVerQueryValue (verInfo, "\\", (LPVOID)&fileVersion, &dummy))
				{
					if (strstr (lowered, "ref_gl"))
						versionedGL = TRUE;
					sprintf (verString, "%d.%d.%d.%d", HIWORD(fileVersion->dwFileVersionMS), LOWORD(fileVersion->dwFileVersionMS), HIWORD(fileVersion->dwFileVersionLS), LOWORD(fileVersion->dwFileVersionLS));
				}
				else
				{
					strcpy (verString, "unknown");
				}
			}
			else
			{
				strcpy (verString, "unknown");
			}

			LocalFree (verInfo);
		}
		else
		{
			strcpy (verString, "unknown");
		}	
	}
	else
	{
		strcpy (verString, "unknown");
	}	

#ifdef _M_AMD64
	fprintf (fhReport, "[0x%16I64X - 0x%16I64X] %s (%lu bytes, version %s)\r\n", ModuleBase, ModuleBase + (DWORD64)ModuleSize, ModuleName, ModuleSize, verString);
#else
	fprintf (fhReport, "[0x%08I64X - 0x%08I64X] %s (%lu bytes, version %s)\r\n", ModuleBase, ModuleBase + (DWORD64)ModuleSize, ModuleName, ModuleSize, verString);
#endif
	return TRUE;
}

BOOL CALLBACK EnumerateLoadedModulesProcSymInfo (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	IMAGEHLP_MODULE64	symInfo = {0};
	FILE				*fhReport = (FILE *)UserContext;
	//CHAR				szImageName[512];
	PCHAR				symType;

	symInfo.SizeOfStruct = sizeof(symInfo);

	if (fnSymGetModuleInfo64 (GetCurrentProcess(), ModuleBase, &symInfo))
	{
		//WideCharToMultiByte (CP_UTF8, 0, symInfo.LoadedImageName, -1, szImageName, sizeof(szImageName), 0, NULL);

		switch (symInfo.SymType)
		{
		case SymCoff:
			symType = "COFF";
			break;
		case SymCv:
			symType = "CV";
			break;
		case SymExport:
			symType = "Export";
			break;
		case SymPdb:
			symType = "PDB";
			break;
		case SymNone:
			symType = "No";
			break;
		default:
			symType = "Unknown";
			break;
		}

		fprintf (fhReport, "%s, %s symbols loaded.\r\n", symInfo.LoadedImageName, symType);
	}
	else
	{
		int i = GetLastError ();
		fprintf (fhReport, "%s, couldn't check symbols (error %d, DBGHELP.DLL too old?)\r\n", ModuleName, i);
	}
	
	return TRUE;
}

CHAR	szModuleName[MAX_PATH];

BOOL CALLBACK EnumerateLoadedModulesProcInfo (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	DWORD64	addr = (DWORD64)UserContext;
	if (addr > ModuleBase && addr < ModuleBase + ModuleSize)
	{
		strncpy (szModuleName, ModuleName, sizeof(szModuleName)-1);
		return FALSE;
	}
	return TRUE;
}

#ifdef USE_CURL
static int EXPORT R1Q2UploadProgress (void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	char	progressBuff[512];
	DWORD	len, ret;
	double	speed;
	int		percent;

	if (ultotal <= 0)
		return 0;

	curl_easy_getinfo ((CURL *)clientp, CURLINFO_SPEED_UPLOAD, &speed);

	percent = (int)((ulnow / ultotal)*100.0f);

	len = snprintf (progressBuff, sizeof(progressBuff)-1, "[%d%%] %g / %g bytes, %g bytes/sec.\n", percent, ulnow, ultotal, speed);
	WriteConsole(GetStdHandle (STD_OUTPUT_HANDLE), progressBuff, len, &ret, NULL);

	snprintf (progressBuff, sizeof(progressBuff)-1, "[%d%%] R1Q2 Crash Dump Uploader", percent);
	SetConsoleTitle (progressBuff);

	return 0;
}

#ifndef DEDICATED_ONLY
extern cvar_t	*cl_http_proxy;
#endif
VOID R1Q2UploadCrashDump (LPCSTR crashDump, LPCSTR crashText)
{
	struct curl_httppost* post = NULL;
	struct curl_httppost* last = NULL;

	CURL	*curl;

	DWORD	lenDmp;
	DWORD	lenTxt;
	DWORD	ret;

	BOOL	console = FALSE;

	__try
	{
		lenDmp = Sys_FileLength (crashDump);
		lenTxt = Sys_FileLength (crashText);

		if (lenTxt == -1)
			return;

		if (AllocConsole ())
			console = TRUE;

		SetConsoleTitle ("R1Q2 Crash Dump Uploader");

		if (console)
			WriteConsole(GetStdHandle (STD_OUTPUT_HANDLE), "Connecting...\n", 14, &ret, NULL);

		curl = curl_easy_init ();

		/* Add simple file section */
		if (lenDmp > 0)
			curl_formadd (&post, &last, CURLFORM_PTRNAME, "minidump", CURLFORM_FILE, crashDump, CURLFORM_END);

		curl_formadd (&post, &last, CURLFORM_PTRNAME, "report", CURLFORM_FILE, crashText, CURLFORM_END);

		/* Set the form info */
		curl_easy_setopt (curl, CURLOPT_HTTPPOST, post);

#ifndef DEDICATED_ONLY
		if (cl_http_proxy)
			curl_easy_setopt (curl, CURLOPT_PROXY, cl_http_proxy->string);
#endif

		//curl_easy_setopt (curl, CURLOPT_UPLOAD, 1);
		if (console)
		{
			curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, R1Q2UploadProgress);
			curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, curl);
			curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0);
		}
		else
		{
			curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 1);
		}

		curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt (curl, CURLOPT_USERAGENT, R1Q2_VERSION_STRING);
		curl_easy_setopt (curl, CURLOPT_URL, "http://www.r1ch.net/stuff/r1q2/receiveCrashDump.php");

		if (curl_easy_perform (curl) != CURLE_OK)
		{
			if (!win_silentexceptionhandler->intvalue)
				MessageBox (NULL, "An error occured while trying to upload the crash dump. Please post it manually on the R1Q2 forums.", "Upload Error", MB_ICONEXCLAMATION | MB_OK);
		}
		else
		{
			if (!win_silentexceptionhandler->intvalue)
			{
				long response;

				if (curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &response) == CURLE_OK)
				{
					if (response == 202)
					{
						MessageBox (NULL, "Upload completed, however the server reports that you are using an out of date R1Q2 version. Crash reports from old versions are not as likely to be investigated as the problem may already have been fixed. Please update your R1Q2 using the R1Q2Updater.", "Upload Complete", MB_ICONEXCLAMATION | MB_OK);
					}
					else if (response == 200)
					{
						MessageBox (NULL, "Upload completed. Thanks for submitting your crash report!\n\nIf you would like feedback on the cause of this crash, please post a brief note on the R1Q2 forums describing what you were doing at the time the exception occured. If possible, please also attach the R1Q2CrashLog.txt file.", "Upload Complete", MB_ICONINFORMATION | MB_OK);
					}
					else
					{
						MessageBox (NULL, "Upload failed, HTTP error.", "Upload Failed", MB_ICONEXCLAMATION | MB_OK);
					}
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		if (!win_silentexceptionhandler->intvalue)
			MessageBox (NULL, "An exception occured while trying to upload the crash dump. Please post it manually on the R1Q2 forums.", "Upload Error", MB_ICONEXCLAMATION | MB_OK);
	}

	if (console)
		FreeConsole ();
}
#endif

BOOL IsOpenGLValid (VOID)
{
	HMODULE	hOpenGL;

	hOpenGL = GetModuleHandle ("OPENGL32");
	if (hOpenGL)
	{
		CHAR	openglPath[MAX_PATH];
		CHAR	systemPath[MAX_PATH];

		GetModuleFileName (hOpenGL, openglPath, sizeof(openglPath)-1);
		GetSystemDirectory (systemPath, sizeof(systemPath)-1);

		strlwr (openglPath);
		strlwr (systemPath);
		
		if (strstr (openglPath, systemPath))
			return TRUE;
	}

	return FALSE;
}

DWORD R1Q2ExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo)
{
	FILE	*fhReport;

	HANDLE	hProcess;

	HMODULE	hDbgHelp, hVersion;

	MINIDUMP_EXCEPTION_INFORMATION miniInfo;
	STACKFRAME64	frame = {0};
	CONTEXT			context = *exceptionInfo->ContextRecord;
	SYMBOL_INFO		*symInfo;
	DWORD64			fnOffset;
	CHAR			tempPath[MAX_PATH];
	CHAR			dumpPath[MAX_PATH];
	OSVERSIONINFOEX	osInfo;
	SYSTEMTIME		timeInfo;

	ENUMERATELOADEDMODULES64	fnEnumerateLoadedModules64;
	SYMSETOPTIONS				fnSymSetOptions;
	SYMINITIALIZE				fnSymInitialize;
	STACKWALK64					fnStackWalk64;
	SYMFUNCTIONTABLEACCESS64	fnSymFunctionTableAccess64;
	SYMGETMODULEBASE64			fnSymGetModuleBase64;
	SYMFROMADDR					fnSymFromAddr;
	SYMCLEANUP					fnSymCleanup;
	MINIDUMPWRITEDUMP			fnMiniDumpWriteDump;

	DWORD						ret, i;
	DWORD64						InstructionPtr;

	BOOL						wantUpload = TRUE;

	CHAR						searchPath[MAX_PATH], *p, *gameMsg;

	if (win_disableexceptionhandler->intvalue == 2)
		return EXCEPTION_EXECUTE_HANDLER;
	else if (win_disableexceptionhandler->intvalue)
		return EXCEPTION_CONTINUE_SEARCH;

#ifndef DEDICATED_ONLY
	ShowCursor (TRUE);
	if (cl_hwnd)
		DestroyWindow (cl_hwnd);
#else
	if (hwnd_Server)
		EnableWindow (hwnd_Server, FALSE);
#endif

#ifdef _DEBUG
	ret = MessageBox (NULL, "EXCEPTION_CONTINUE_SEARCH?", "Unhandled Exception", MB_ICONERROR | MB_YESNO);
	if (ret == IDYES)
		return EXCEPTION_CONTINUE_SEARCH;
#endif

#ifndef _DEBUG
	if (IsDebuggerPresent ())
		return EXCEPTION_CONTINUE_SEARCH;
#endif

	hDbgHelp = LoadLibrary ("DBGHELP");
	hVersion = LoadLibrary ("VERSION");

	if (!hDbgHelp)
	{
		if (!win_silentexceptionhandler->intvalue)
			MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since R1Q2 failed to load DBGHELP.DLL. Please obtain DBGHELP.DLL and place it in your R1Q2 directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (hVersion)
	{
		fnVerQueryValue = (VERQUERYVALUE)GetProcAddress (hVersion, "VerQueryValueA");
		fnGetFileVersionInfo = (GETFILEVERSIONINFO)GetProcAddress (hVersion, "GetFileVersionInfoA");
		fnGetFileVersionInfoSize = (GETFILEVERSIONINFOSIZE)GetProcAddress (hVersion, "GetFileVersionInfoSizeA");
	}

	fnEnumerateLoadedModules64 = (ENUMERATELOADEDMODULES64)GetProcAddress (hDbgHelp, "EnumerateLoadedModules64");
	fnSymSetOptions = (SYMSETOPTIONS)GetProcAddress (hDbgHelp, "SymSetOptions");
	fnSymInitialize = (SYMINITIALIZE)GetProcAddress (hDbgHelp, "SymInitialize");
	fnSymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64)GetProcAddress (hDbgHelp, "SymFunctionTableAccess64");
	fnSymGetModuleBase64 = (SYMGETMODULEBASE64)GetProcAddress (hDbgHelp, "SymGetModuleBase64");
	fnStackWalk64 = (STACKWALK64)GetProcAddress (hDbgHelp, "StackWalk64");
	fnSymFromAddr = (SYMFROMADDR)GetProcAddress (hDbgHelp, "SymFromAddr");
	fnSymCleanup = (SYMCLEANUP)GetProcAddress (hDbgHelp, "SymCleanup");
	fnSymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress (hDbgHelp, "SymGetModuleInfo64");
	//fnSymLoadModule64 = (SYMLOADMODULE64)GetProcAddress (hDbgHelp, "SymLoadModule64");
	fnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress (hDbgHelp, "MiniDumpWriteDump");

	if (!fnEnumerateLoadedModules64 || !fnSymSetOptions || !fnSymInitialize || !fnSymFunctionTableAccess64 ||
		!fnSymGetModuleBase64 || !fnStackWalk64 || !fnSymFromAddr || !fnSymCleanup || !fnSymGetModuleInfo64)// ||
		//!fnSymLoadModule64)
	{
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since the version of DBGHELP.DLL in use is too old. Please obtain an up-to-date DBGHELP.DLL and place it in your R1Q2 directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (!win_silentexceptionhandler->intvalue)
		ret = MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. Would you like to generate a crash report?", "Unhandled Exception", MB_ICONEXCLAMATION | MB_YESNO);
	else
		ret = IDYES;

	if (ret == IDNO)
	{
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	hProcess = GetCurrentProcess();

	fnSymSetOptions (SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_ANYTHING);

	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p) p[0] = 0;

	GetSystemTime (&timeInfo);

	i = 1;

	for (;;)
	{
		snprintf (tempPath, sizeof(tempPath)-1, "%s\\R1Q2CrashLog%.4d-%.2d-%.2d%_%d.txt", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
		if (Sys_FileLength (tempPath) == -1)
			break;
		i++;
	}

	fhReport = fopen (tempPath, "wb");

	if (!fhReport)
	{
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

#ifdef _DEBUG
	//strcat (searchPath, ";c:\\websyms");
#endif

	fnSymInitialize (hProcess, searchPath, TRUE);

#ifdef _DEBUG
	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p) p[0] = 0;
#endif


#ifdef _M_AMD64
	InstructionPtr = context.Rip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrStack.Offset = context.Rsp;
#else
	InstructionPtr = context.Eip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif

	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	symInfo = LocalAlloc (LPTR, sizeof(*symInfo) + 128);
	symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	symInfo->MaxNameLen = 128;
	fnOffset = 0;

	memset (&osInfo, 0, sizeof(osInfo));
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx ((OSVERSIONINFO *)&osInfo))
	{
		osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx ((OSVERSIONINFO *)&osInfo);
	}

	strcpy (szModuleName, "<unknown>");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)InstructionPtr);

	strlwr (szModuleName);

	if (strstr (szModuleName, "gamex86.dll"))
	{
		gameMsg = 
			"It is very likely that the Game DLL you are using for the mod you run is at fault.\r\n"
			"Please send this crash report to the author(s) of the mod you are running.";
		wantUpload = FALSE;
	}
	else if (strstr (szModuleName, "ref_soft.dll"))
	{
		gameMsg = 
			"It is very likely that the software renderer (ref_soft) is at fault. Software\r\n"
			"rendering in Q2 has always been unreliable. If possible, please use OpenGL to avoid\r\n"
			"crashes. Consider using the modified ref_soft.dll available on the r1ch.net forums to\r\n"
			"fix some small bugs if you are not using it already.";
		wantUpload = FALSE;
	}
	else if (strstr (szModuleName, "opengl32.dll"))
	{
		if (!IsOpenGLValid ())
		{
			gameMsg = 
				"You have an OPENGL32.DLL file in your Quake II folder. This is overriding the correct\r\n"
				"DLL from your Windows system folder. Delete the OPENGL32.DLL in your Quake II folder to\r\n"
				"ensure the correct version of OPENGL is loaded.";
			wantUpload = FALSE;
		}
		else
		{
			gameMsg = 
				"This crash occured in OpenGL. Make sure you are using the latest video drivers and\r\n"
				"the latest version of R1GL. If you are using software mode, consider switching to\r\n"
				"OpenGL for better performance.";
		}
	}
	else if (strstr (szModuleName, "ref_gl.dll") && !versionedGL)
	{
		gameMsg = 
			"This crash occured in ref_gl and you aren't using R1GL. It is unlikely that\r\n"
			"this crash can be fixed. Consider switching to R1GL for improved crash\r\n"
			"reporting, speed and stability. The original ref_gl has known crash bugs that\r\n"
			"cannot be fixed so you will continue to crash unless you use R1GL.";
		wantUpload = FALSE;
	}
	else if (strstr (szModuleName, "r1q2.exe") || strstr (szModuleName, "ref_r1gl.dll") || strstr (szModuleName, "dedicated.exe"))
	{
#ifdef USE_CURL
		gameMsg = 
		"Since this crash appears to be inside R1Q2 or R1GL, it would be very helpful\r\n"
		"if when prompted, you submitted the crash report to r1ch.net. This will aid in\r\n"
		"finding the fault that caused this exception.";
#else
		gameMsg = 
		"\r\nSince this crash appears to be inside R1Q2 or R1GL, it would be very helpful\r\n"
		"if you submitted the crash report to r1ch.net forums. This will aid in finding\r\n"
		"the fault that caused this exception.";
#endif
	}
	else
	{
		gameMsg =
		"Please note, unless you are using both R1Q2 and R1GL, any crashes will be much\r\n"
		"harder to diagnose. If you are still using ref_gl, please consider using R1GL for\r\n"
		"an accurate crash report.";
	}

#ifdef USE_CURL
	fprintf (fhReport,
		PRODUCTNAME " encountered an unhandled exception and has terminated. If you are able to\r\n"
		"reproduce this crash, please submit the crash report to r1ch.net when prompted or\r\n"
		"post this file and the crash dump .dmp file (if available) on the R1Q2 forums at\r\n"
		"http://www.r1ch.net/forum/index.php?board=8.0\r\n"
		"\r\n"
		"     PLEASE MAKE SURE YOU ARE USING THE LATEST VERSIONS OF R1Q2/R1GL/ETC!\r\n"
		"\r\n"
		"This crash appears to have occured in the '%s' module.\r\n%s\r\n\r\n", szModuleName, gameMsg);
#else
	fprintf (fhReport,
		PRODUCTNAME " encountered an unhandled exception and has terminated. If you are able to\r\n"
		"reproduce this crash, please submit the crash report on the R1Q2 forums at\r\n"
		"http://www.r1ch.net/forum/index.php?board=8.0 - include this .txt file and the\r\n"
		".dmp file (if available)\r\n"
		"\r\n"
		"This crash appears to have occured in the '%s' module.%s\r\n\r\n", szModuleName, gameMsg);
#endif

	fprintf (fhReport, "**** UNHANDLED EXCEPTION: %x\r\nFault address: %I64p (%s)\r\n", exceptionCode, InstructionPtr, szModuleName);

	fprintf (fhReport, PRODUCTNAME " module: %s(%s) (Version: %s)\r\n", binary_name, R1BINARY, R1Q2_VERSION_STRING);
	fprintf (fhReport, "Windows version: %d.%d (Build %d) %s\r\n\r\n", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.szCSDVersion);

	fprintf (fhReport, "Symbol information:\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcSymInfo, (VOID *)fhReport);

	fprintf (fhReport, "\r\nEnumerate loaded modules:\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcDump, (VOID *)fhReport);

	fprintf (fhReport, "\r\nStack trace:\r\n");
	fprintf (fhReport, "Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n");
	while (fnStackWalk64 (IMAGE_FILE_MACHINE_I386, hProcess, GetCurrentThread(), &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL))
	{
		strcpy (szModuleName, "<unknown>");
		fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)frame.AddrPC.Offset);
		strlwr (szModuleName);

		p = strrchr (szModuleName, '\\');
		if (p)
		{
			p++;
		}
		else
		{
			p = szModuleName;
		}

		if (strstr (p, "ref_gl.dll") && !versionedGL)
		{
			gameMsg = 
				"This crash occured in ref_gl and you aren't using R1GL. It is unlikely that "
				"this crash can be fixed. Consider switching to R1GL for improved stability, speed "
				"and crash reporting capabilities. The original ref_gl has known crash bugs that "
				"cannot be fixed so you will continue to crash unless you use R1GL.";
			wantUpload = FALSE;
		}
		else if (strstr (p, "ref_ncgl.dll"))
		{
			gameMsg = 
				"This crash occured in ref_ncgl.dll. It is unlikely that this crash can be fixed. "
				"Consider switching to R1GL for improved stability, speed and crash reporting capabilities. "
				"NoCheat GL has known crash bugs that are not fixed so you will continue to crash unless you "
				"use R1GL.";
			wantUpload = FALSE;
		}
		else if (strstr (p, "atioglxx.dll") || strstr (p, "atioglx2.dll"))
		{
			gameMsg = 
				"This crash occured in the ATI GL drivers. The ATI drivers are optimized in such "
				"a way that makes debugging very difficult. Please make sure you are using the latest "
				"ATI drivers and that you do not have any OPENGL32.DLL or ATIOGLXX.DLL files in your Q2 "
				"folder.";
			wantUpload = FALSE;
		}
		else if (strstr (p, "opengl32.dll"))
		{
			if (!IsOpenGLValid ())
			{
				gameMsg = 
					"You have an OPENGL32.DLL file in your Quake II folder. This is overriding the correct "
					"DLL from your Windows system folder. Delete the OPENGL32.DLL in your Quake II folder to "
					"ensure the correct version of OPENGL is loaded.";
				wantUpload = FALSE;
			}
		}

#ifdef _M_AMD64
		if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
		{
			fprintf (fhReport, "%16I64X %16I64X %16I64X %16I64X %16I64X %16I64X %s!%s+0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, frame.Params[0], frame.Params[1], frame.Params[2], frame.Params[3], p, symInfo->Name, fnOffset);
		}
		else
		{
			fprintf (fhReport, "%16I64X %16I64X %16I64X %16I64X %16I64X %16I64X %s!0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, frame.Params[0], frame.Params[1], frame.Params[2], frame.Params[3], p, frame.AddrPC.Offset);
		}
#else
		if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
		{
			fprintf (fhReport, "%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!%s+0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, symInfo->Name, fnOffset);
		}
		else
		{
			fprintf (fhReport, "%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, frame.AddrPC.Offset);
		}
#endif
	}

	if (fnMiniDumpWriteDump)
	{
		HANDLE	hFile;

		GetTempPath (sizeof(dumpPath)-16, dumpPath);
		strcat (dumpPath, "R1Q2CrashDump.dmp");

		hFile = CreateFile (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			miniInfo.ClientPointers = TRUE;
			miniInfo.ExceptionPointers = exceptionInfo;
			miniInfo.ThreadId = GetCurrentThreadId ();
			if (fnMiniDumpWriteDump (hProcess, GetCurrentProcessId(), hFile, MiniDumpWithIndirectlyReferencedMemory|MiniDumpWithDataSegs, &miniInfo, NULL, NULL))
			{
#ifndef NO_ZLIB
				FILE	*fh;
#endif
				CHAR	zPath[MAX_PATH];

				CloseHandle (hFile);
#ifndef NO_ZLIB
				fh = fopen (dumpPath, "rb");
				if (fh)
				{
					
					BYTE	buff[0xFFFF];
					size_t	len;
					gzFile	gz;

					snprintf (zPath, sizeof(zPath)-1, "%s\\R1Q2CrashLog%.4d-%.2d-%.2d_%d.dmp.gz", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
					gz = gzopen (zPath, "wb");
					if (gz)
					{
						while ((len = fread (buff, 1, sizeof(buff), fh)) > 0)
						{
							gzwrite (gz, buff, (unsigned int)len);
						}
						gzclose (gz);
						fclose (fh);
					}
				}
#else
				snprintf (zPath, sizeof(zPath)-1, "%s\\R1Q2CrashLog%.4d-%.2d-%.2d_%d.dmp", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
				CopyFile (dumpPath, zPath, FALSE);
#endif			
				DeleteFile (dumpPath);
				strcpy (dumpPath, zPath);
				fprintf (fhReport, "\r\nA minidump was saved to %s.\r\nPlease include this file when posting a crash report.\r\n", dumpPath);
			}
			else
			{
				CloseHandle (hFile);
				DeleteFile (dumpPath);
			}
		}
	}
	else
	{
		fprintf (fhReport, "\r\nA minidump could not be created. Minidumps are only available on Windows XP or later.\r\n");
	}

	fclose (fhReport);

	LocalFree (symInfo);

	fnSymCleanup (hProcess);

	if (!win_silentexceptionhandler->intvalue)
	{
		HMODULE shell;
		shell = LoadLibrary ("SHELL32");
		if (shell)
		{
			SHELLEXECUTEA fncOpen = (SHELLEXECUTEA)GetProcAddress (shell, "ShellExecuteA");
			if (fncOpen)
                fncOpen (NULL, NULL, tempPath, NULL, searchPath, SW_SHOWDEFAULT);

			FreeLibrary (shell);
		}
	}

#ifdef USE_CURL
	if (wantUpload)
	{
		if (!win_silentexceptionhandler->intvalue)
			ret = MessageBox (NULL, "Would you like to upload this crash report to r1ch.net to help improve R1Q2? If you are able to reproduce this crash, please do not submit multiple reports as this will only delay processing.\r\n\r\nIf you would like feedback on this crash, please post the crash report and .dmp file on the r1ch.net forums.", "Unhandled Exception", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
		else
			ret = IDYES;

		if (ret == IDYES)
			R1Q2UploadCrashDump (dumpPath, tempPath);
	}
	else
	{
		CHAR message[1024], *s;
		strcpy (message, "Crash analysis:\r\n\r\n");
		strcat (message, gameMsg);
		s = message + sizeof("Crash analysis:\r\n\r\n");
		while (s[0])
		{
			if (s[0] == '\n')
				s[0] = ' ';
			else if (s[0] == '\r')
				s[0] = ' ';
			s++;
		}

		MessageBox (NULL, message, "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
	}
#endif

	FreeLibrary (hDbgHelp);
	if (hVersion)
		FreeLibrary (hVersion);

	return EXCEPTION_EXECUTE_HANDLER;
}

#ifndef DEDICATED_ONLY

char	sys_url_location[1024];

void Sys_ShellExec (const char *cmd)
{
	HMODULE shell;
	shell = LoadLibrary ("SHELL32");
	if (shell)
	{
		SHELLEXECUTEA fncOpen = (SHELLEXECUTEA)GetProcAddress (shell, "ShellExecuteA");
		if (fncOpen)
            fncOpen (NULL, NULL, cmd, NULL, NULL, SW_SHOWDEFAULT);

		FreeLibrary (shell);
	}
}

void Sys_OpenURL (void)
{
	Sys_ShellExec (sys_url_location);
}

void Sys_UpdateURLMenu (const char *s)
{
	HMENU	menu;
	CHAR	title[80];
	CHAR	*dots;

	GetSystemMenu (cl_hwnd, TRUE);

	if (strlen (s) > 64)
		dots = "...";
	else
		dots = "";

	strncpy (sys_url_location, s, sizeof(sys_url_location)-1);

	Com_sprintf (title, sizeof(title), "Open \"%.64s%s\"", s, dots);

	menu = GetSystemMenu (cl_hwnd, FALSE);
	InsertMenu (menu, 0,  MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu (menu, 0,  MF_BYPOSITION, 1234, title);
}
#endif

const __int64 nano100SecInWeek= (__int64)10000000*60*60*24*7;
const __int64 nano100SecInDay = (__int64)10000000*60*60*24;
const __int64 nano100SecInHour= (__int64)10000000*60*60;
const __int64 nano100SecInMin = (__int64)10000000*60;
const __int64 nano100SecInSec = (__int64)10000000;

void Sys_ProcessTimes_f (void)
{
	FILETIME		createTime, exitTime, kernelTime, userTime;
	__int64			total, tmp;
	DWORD			days, hours, mins;
	double			seconds;

	GetProcessTimes (GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime);

	total = *(__int64 *)&kernelTime;

	tmp = total / nano100SecInDay;
	days = (DWORD)tmp;
	total -= tmp * nano100SecInDay;

	tmp = total / nano100SecInHour;
	hours = (DWORD)tmp;
	total -= tmp * nano100SecInHour;

	tmp = total / nano100SecInMin;
	mins = (DWORD)tmp;
	total -= tmp * nano100SecInMin;

	seconds = (double)total / (double)nano100SecInSec;
	
	Com_Printf ("%ud %uh %um %gs kernel\n", LOG_GENERAL, days, hours, mins, seconds);

	total = *(__int64 *)&userTime;

	tmp = total / nano100SecInDay;
	days = (DWORD)tmp;
	total -= tmp * nano100SecInDay;

	tmp = total / nano100SecInHour;
	hours = (DWORD)tmp;
	total -= tmp * nano100SecInHour;

	tmp = total / nano100SecInMin;
	mins = (DWORD)tmp;
	total -= tmp * nano100SecInMin;

	seconds = (double)total / (double)nano100SecInSec;

	Com_Printf ("%ud %uh %um %gs user\n", LOG_GENERAL, days, hours, mins, seconds);
}

static unsigned int badspins, goodspins;

void Sys_Spinstats_f (void)
{
	Com_Printf ("%u fast spins, %u slow spins, %.2f%% slow.\n", LOG_GENERAL, goodspins, badspins, ((float)badspins / (float)(goodspins+badspins)) * 100.0f);
}

#ifdef _M_IX86

__declspec(naked) unsigned short Sys_GetFPUStatus (void)
{
	__asm
	{
		xor eax, eax
		push eax
		mov eax, esp
		fnstcw dword ptr [eax]
		pop eax
		ret
	}
}

void Sys_SetFPU (byte bits)
{
	__asm
	{
		xor eax, eax
		push eax
		mov eax, esp
		mov ecx, eax
		fnstcw word ptr [eax]
		mov eax, [eax]
		and ah, 0f0h
		or  ah, bits          ; RTZ/truncate/chop mode, 24 bit precision
		mov [ecx], eax
		fldcw word ptr [ecx]
		pop eax
	}
}

//FPU should be round to nearest, 24 bit precision.
//3.20 = 0x007f
qboolean Sys_CheckFPUStatus (void)
{
	static unsigned short	last_word = 0;
	unsigned short	fpu_control_word;

	fpu_control_word = Sys_GetFPUStatus ();

	Com_DPrintf ("Sys_CheckFPUStatus: rounding %d, precision %d\n", (fpu_control_word >> 10) & 3, (fpu_control_word >> 8) & 3);

	//check rounding (10) and precision (8) are set properly
	/*if (((fpu_control_word >> 10) & 3) != 3 ||
		((fpu_control_word >> 8) & 3) != 0)
	{
		if (fpu_control_word != last_word)
		{
			Com_Printf ("\2WARNING: The FPU control word was modified by some external force to rounding %d, precision %d. Resetting.\n", LOG_GENERAL, (fpu_control_word >> 10) & 3, (fpu_control_word >> 8) & 3);
			Sys_SetFPU (sys_fpu_bits->intvalue);
			fpu_control_word = Sys_GetFPUStatus ();
			last_word = fpu_control_word;
			return false;
		}
	}*/

	last_word = fpu_control_word;
	return true;
}
#endif

/*
==================
WinMain

==================
*/
HINSTANCE	global_hInstance;

//#define FLOAT2INTCAST(f)(*((int *)(&f)))
//#define FLOAT_GT_ZERO(f) (FLOAT2INTCAST(f) > 0)

extern cvar_t	*sys_loopstyle;
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifndef NO_SERVER
//	unsigned int	handle;
#endif
    MSG				msg;
	unsigned int	time, oldtime, newtime;
	int				spins;

	badspins = goodspins = 0;

    /* previous instances do not exist in Win32 */
    if (hPrevInstance)
        return 0;

	if (hInstance)
		strncpy (cmdline, lpCmdLine, sizeof(cmdline)-1);

	global_hInstance = hInstance;

	ParseCommandLine (lpCmdLine);

	//r1ch: always change to our directory (ugh)
	FixWorkingDirectory ();

	//hInstance is empty when we are back here with service code
#ifdef DEDICATED_ONLY
	if (hInstance && argc > 1)
	{
		if (!strcmp(argv[1], "-service"))
		{
			global_Service = true;
			return main ();
		}
	}
#endif


	__try
	{
		Sys_SetFPU (sys_fpu_bits->intvalue);
		Sys_CheckFPUStatus ();

		Qcommon_Init (argc, argv);

#ifndef _M_AMD64
		//_controlfp( _PC_24, _MCW_PC );
#endif

		oldtime = Sys_Milliseconds ();
		/* main window message loop */
		for (;;)
		{
			// if at a full screen console, don't update unless needed
			//if (Minimized
	/*#ifndef NO_SERVER			
				|| (dedicated->intvalue)
	#endif*/
			//)
			//{
			//	Sleep (1);
			//}

			/*while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
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
			}*/

			if (dedicated->intvalue && sys_loopstyle->intvalue)
			{
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
				newtime = Sys_Milliseconds ();
				time = newtime - oldtime;
				spins = 0;
			}
			else
			{
				spins = 0;
				do
				{
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
					newtime = Sys_Milliseconds ();
					time = newtime - oldtime;
					if (!time)
						Sleep (0);
					spins ++;
				} while (0 && time < 1);
			}

			if (spins > 500)
				badspins++;
			else
				goodspins++;

			Sys_SetFPU (sys_fpu_bits->intvalue);
			//Sys_CheckFPUStatus ();
			Qcommon_Frame (time);

			oldtime = newtime;
		}
	}
	__except (R1Q2ExceptionHandler(GetExceptionCode(), GetExceptionInformation()))
	{
		return 1;
	}

	return 0;
}
