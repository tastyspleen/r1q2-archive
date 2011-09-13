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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0400
#include "resource.h"
#include "..\client\client.h"
#include "winquake.h"
//#include "zmouse.h"

// Structure containing functions exported from refresh DLL
refexport_t	re;

static HWND old_hwnd = 0;

cvar_t *win_noalttab;
//cvar_t *win_nopriority;

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL (WM_MOUSELAST+1)  // message that will be supported by the OS 
#endif

static UINT MSH_MOUSEWHEEL;

// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_ref;			// Name of Refresh DLL loaded
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;
cvar_t		*vid_quietload;

cvar_t		*vid_noexceptionhandler = &uninitialized_cvar;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules
HINSTANCE	reflib_library;		// Handle to refresh DLL 
qboolean	reflib_active = false;

HWND        cl_hwnd;            // Main window handle for life of program

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static qboolean s_alttab_disabled;

extern	uint32	sys_msg_time;

/*
** WIN32 helper functions
*/
extern qboolean s_win95;

qboolean closing_reflib;

static void WIN_DisableAltTab( void )
{
	if ( s_alttab_disabled )
		return;

	if ( s_win95 )
	{
		BOOL old;

		SystemParametersInfo( SPI_SCREENSAVERRUNNING, 1, &old, 0 );
	}
	else
	{
		RegisterHotKey( 0, 0, MOD_ALT, VK_TAB );
		RegisterHotKey( 0, 1, MOD_ALT, VK_RETURN );
	}
	s_alttab_disabled = true;
}

static void WIN_EnableAltTab( void )
{
	if ( s_alttab_disabled )
	{
		if ( s_win95 )
		{
			BOOL old;

			SystemParametersInfo( SPI_SCREENSAVERRUNNING, 0, &old, 0 );
		}
		else
		{
			UnregisterHotKey( 0, 0 );
			UnregisterHotKey( 0, 1 );
		}

		s_alttab_disabled = false;
	}
}

/*
==========================================================================

DLL GLUE

==========================================================================
*/
int ignore_vidprintf = 0;

#define	MAXPRINTMSG	4096
void EXPORT VID_Printf (int print_level, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	//static qboolean	inupdate;

	if (ignore_vidprintf)
		return;
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt, argptr);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	if (print_level == PRINT_ALL)
	{
		Com_Printf ("%s", LOG_CLIENT, msg);
	}
	else if ( print_level == PRINT_DEVELOPER )
	{
		Com_DPrintf ("%s", msg);
	}
	else if ( print_level == PRINT_ALERT )
	{
		MessageBox( 0, msg, "PRINT_ALERT", MB_ICONWARNING );
		OutputDebugString( msg );
	}
}

void EXPORT VID_Error (int err_level, const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	//static qboolean	inupdate;
	
	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg)-1, fmt,argptr);
	va_end (argptr);

	msg[sizeof(msg)-1] = 0;

	Com_Error (err_level,"%s", msg);
}

//==========================================================================

const byte        scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10,  K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,K_KP_MINUS,K_LEFTARROW,K_KP_5,K_RIGHTARROW, K_KP_PLUS,K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    K_APP  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0
};
// 7 

const byte		dinputkeymap[256] = 
{
	0,			27,			'1',		'2',		'3',		'4',		'5',		'6',		//0-7
	'7',		'8',		'9',		'0',		'-',		'=',		K_BACKSPACE,'\t',		//8-15
	'q',		'w',		'e',		'r',		't',		'y',		'u',		'i',		//16-23
	'o',		'p',		'[',		']',		13,			K_CTRL,		'a',		's',		//24-31
	'd',		'f',		'g',		'h',		'j',		'k',		'l',		';',		//32-39
	'\'',		'`',		K_SHIFT,	'\\',		'z',		'x',		'c',		'v',		//40-47
	'b',		'n',		'm',		',',		'.',		'/',		K_SHIFT,	'*',		//48-55
	K_ALT,		' ',		K_CAPSLOCK,	K_F1,		K_F2,		K_F3,		K_F4,		K_F5,		//56-63
	K_F6,		K_F7,		K_F8,		K_F9,		K_F10,		K_NUMLOCK,	K_SCROLLLOCK,K_KP_HOME,	//64-71
	K_KP_UPARROW,K_KP_PGUP,	K_KP_MINUS,	K_KP_LEFTARROW,K_KP_5,	K_KP_RIGHTARROW,K_KP_PLUS,K_KP_END,	//72-79
	K_KP_DOWNARROW,K_KP_PGDN,K_KP_INS,	K_KP_DEL,	0,			0,			'\\',			K_F11,		//80-87
	K_F12,		0,			0,			0,			0,			0,			0,			0,			//88-95
	0,			0,			0,			0,			0,			0,			0,			0,			//96-103
	0,			0,			0,			0,			0,			0,			0,			0,			//104-111
	0,			0,			0,			0,			0,			0,			0,			0,			//112-119
	0,			0,			0,			0,			0,			0,			0,			0,			//120-127
	//--------------------dinput key mappings below here------------------------
	0,0,		0,			0,			0,			0,			0,			0,			0,			//128-136 FUCK!
	0,			0,			0,			0,			0,			0,			0,			0,			//137-144
	0,			0,			0,			0,			0,			0,			0,			0,			//145-152
	0,			0,			0,			K_KP_ENTER,	K_CTRL,		0,			0,			0,			//160
	0,			0,			0,			0,			0,			0,			0,			0,			//168
	0,			0,			0,			0,			0,			0,			0,			0,			//176
	0,			0,			0,			0,			K_KP_SLASH	,0,			K_PRTSCR,	K_ALT,		//184
	0,			0,			0,			0,			0,			0,			0,			K_HOME,		//192
	0,			0,			0,			0,			K_PAUSE,	0,			K_HOME,		K_UPARROW,	//200
	K_PGUP,		0,			K_LEFTARROW,0,			K_RIGHTARROW,0,			K_END,		K_DOWNARROW,//208
	K_PGDN,		K_INS,		K_DEL,		0,			0,			0,			0,			0,			//216
	0,			0,			0,			0,			0,			0,			0,			0,			//224
	0,			0,			0,			0,			0,			0,			0,			0,			//232
	0,			0,			0,			0,			0,			0,			0,			0,			//240
	0,			0,			0,			0,			0,			0,			0,			0,			//248
	0,			0,			0,			0,			0,			0,			0						//255
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	int result;
	int modified = ( key >> 16 ) & 255;
	qboolean is_extended = false;

	if ( modified > 127)
		return 0;

	if ( key & ( 1 << 24 ) )
		is_extended = true;

	result = scantokey[modified];

	if ( !is_extended )
	{
		switch ( result )
		{
		case K_HOME:
			return K_KP_HOME;
		case K_UPARROW:
			return K_KP_UPARROW;
		case K_PGUP:
			return K_KP_PGUP;
		case K_LEFTARROW:
			return K_KP_LEFTARROW;
		case K_RIGHTARROW:
			return K_KP_RIGHTARROW;
		case K_END:
			return K_KP_END;
		case K_DOWNARROW:
			return K_KP_DOWNARROW;
		case K_PGDN:
			return K_KP_PGDN;
		case K_INS:
			return K_KP_INS;
		case K_DEL:
			return K_KP_DEL;
		default:
			return result;
		}
	}
	else
	{
		switch ( result )
		{
		case 0x0D:
			return K_KP_ENTER;
		case 0x2F:
			return K_KP_SLASH;
		case 0xAF:
			return K_KP_PLUS;
		}
		return result;
	}
}

extern cvar_t *s_focusfree;

void AppActivate(BOOL fActive, BOOL minimize)
{
	Minimized = (qboolean)minimize;

	Key_ClearStates();

	// we don't want to act like we're active if we're minimized
	if (fActive && !Minimized)
		ActiveApp = true;
	else
		ActiveApp = false;

	// minimize/restore mouse-capture on demand
	if (!ActiveApp)
	{
		IN_Activate (false);
#ifdef CD_AUDIO
		CDAudio_Activate (false);
#endif

		if (s_focusfree->intvalue)
			S_Activate (false);

		if ( win_noalttab->intvalue )
		{
			WIN_EnableAltTab();
		}

		//if (!win_nopriority->value)
		//	SetPriorityClass (GetCurrentProcess (), LOW_PRIORITY_CLASS);
	}
	else
	{
		//if (!win_nopriority->value)
		//	SetPriorityClass (GetCurrentProcess (), NORMAL_PRIORITY_CLASS);
		IN_Activate (true);
#ifdef CD_AUDIO
		CDAudio_Activate (true);
#endif

		if (s_focusfree->intvalue)
			S_Activate (true);

		if (win_noalttab->intvalue)
			WIN_DisableAltTab();
	}
}

/*
====================
MainWndProc

main window procedure
====================
*/
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//r1: ignore the flood of messages that occur during vid_restart
	if (closing_reflib)
	{
		//Com_Printf ("Ignored WM_%d\n", uMsg);
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}

	if ( uMsg == MSH_MOUSEWHEEL)
	{
		if ( ( ( int ) wParam ) > 0 )
		{
			if (cls.key_dest == key_console || cls.state <= ca_connected) {
				int i;
				for (i = 0; i < 4; i++) {
					Key_Event( K_PGUP, true, sys_msg_time );
					Key_Event( K_PGUP, false, sys_msg_time );
				}
			} else {
				Key_Event( K_MWHEELUP, true, sys_msg_time );
				Key_Event( K_MWHEELUP, false, sys_msg_time );
			}
		}
		else
		{
			if (cls.key_dest == key_console || cls.state <= ca_connected) {
				int i;
				for (i = 0; i < 4; i++) {
					Key_Event( K_PGDN, true, sys_msg_time );
					Key_Event( K_PGDN, false, sys_msg_time );
				}
			} else {
				Key_Event( K_MWHEELDOWN, true, sys_msg_time );
				Key_Event( K_MWHEELDOWN, false, sys_msg_time );
			}
		}
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_MOUSEWHEEL:
		/*
		** this chunk of code theoretically only works under NT4 and Win98
		** since this message doesn't exist under Win95
		*/
		if (ActiveApp && g_pMouse)
			return TRUE;

		if ( ( int16 ) HIWORD( wParam ) > 0 )
		{
			if (cls.key_dest == key_console || cls.state <= ca_connected) {
				int i;
				for (i = 0; i < 4; i++) {
					Key_Event( K_PGUP, true, sys_msg_time );
					Key_Event( K_PGUP, false, sys_msg_time );
				}
			} else {
				Key_Event( K_MWHEELUP, true, sys_msg_time );
				Key_Event( K_MWHEELUP, false, sys_msg_time );
			}
		}
		else
		{
			if (cls.key_dest == key_console || cls.state <= ca_connected) {
				int i;
				for (i = 0; i < 4; i++) {
					Key_Event( K_PGDN, true, sys_msg_time );
					Key_Event( K_PGDN, false, sys_msg_time );
				}
			} else {
				Key_Event( K_MWHEELDOWN, true, sys_msg_time );
				Key_Event( K_MWHEELDOWN, false, sys_msg_time );
			}
		}
		break;
    
    case WM_ENTERMENULOOP:
        // Release the device, so if we are in exclusive mode the 
        // cursor will reappear
        if( g_pMouse )
        {
			IDirectInputDevice8_Unacquire (g_pMouse);
        }
        break;

    case WM_EXITMENULOOP:
        // Make sure the device is acquired when coming out of a menu loop
        if( g_pMouse )
        {
			IDirectInputDevice8_Acquire (g_pMouse);
        }
        break;

	case WM_HOTKEY:
		return 0;

	case WM_CREATE:
		cl_hwnd = hWnd;

		MSH_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG"); 
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_PAINT:
		SCR_DirtyScreen ();	// force entire screen to update next frame
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_QUIT:
	case WM_CLOSE:
		CL_Quit_f();
		return 0;

	case WM_DESTROY:
		// let sound and input know about this?
		cl_hwnd = NULL;
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_ACTIVATE:
		{
			int	fActive, fMinimized;

			// KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);

			AppActivate( fActive != WA_INACTIVE, fMinimized);

			if ( reflib_active )
				re.AppActivate( !( fActive == WA_INACTIVE ) );
		}
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_SIZE:
		if (wParam  == SIZE_MAXIMIZED)
		{
			if ( vid_fullscreen )
			{
				Com_Printf ("WM_SIZE: Going fullscreen!\n", LOG_CLIENT);
				Cvar_Set( "vid_fullscreen", "1" );
			}
		}
		return 0;

	case WM_MOVE:
		{
			int		xPos, yPos;
			RECT r;
			int		style;

			if (!vid_fullscreen->intvalue)
			{
				xPos = (int16) LOWORD(lParam);    // horizontal position 
				yPos = (int16) HIWORD(lParam);    // vertical position 

				if (!IsIconic (cl_hwnd))
				{
					r.left   = 0;
					r.top    = 0;
					r.right  = 1;
					r.bottom = 1;

					style = GetWindowLong( hWnd, GWL_STYLE );
					AdjustWindowRect( &r, style, FALSE );

					Cvar_SetValue( "vid_xpos", (float)(xPos + r.left));
					Cvar_SetValue( "vid_ypos", (float)(yPos + r.top));
					vid_xpos->modified = false;
					vid_ypos->modified = false;
					if (ActiveApp)
						IN_Activate (true);
				}
			}
		}
        return DefWindowProc (hWnd, uMsg, wParam, lParam);

// this is complicated because Win32 seems to pack multiple mouse events into
// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		{
			int	temp;

			if (g_pMouse)
				break;

			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

//#define MK_XBUTTON1         0x0020
//#define MK_XBUTTON2         0x0040

			if (wParam & 0x0020)
				temp |= 8;

			if (wParam & 0x0040)
				temp |= 16;

			IN_MouseEvent (temp);
		}
		break;

	case WM_SYSCOMMAND:

		if ( wParam == SC_SCREENSAVE )
			return 0;

		if ( wParam == 1234)
		{
			Sys_OpenURL();
			return 0;
		}

        return DefWindowProc (hWnd, uMsg, wParam, lParam);

	case WM_SYSKEYDOWN:
		if ( wParam == 13 )
		{
			if ( vid_fullscreen )
			{
				Com_Printf ("ALT+Enter, setting fullscreen %d.\n", LOG_CLIENT, !vid_fullscreen->intvalue);
				Cvar_SetValue( "vid_fullscreen", (float)!vid_fullscreen->intvalue );
			}
			return 0;
		}
		if (!g_pKeyboard)
			Key_Event( MapKey( (int)lParam ), true, sys_msg_time);
		return 0;
	case WM_KEYDOWN:
		if (!g_pKeyboard)
			Key_Event( MapKey( (int)lParam ), true, sys_msg_time);
		break;

	case WM_SYSKEYUP:
		if (!g_pKeyboard)
			Key_Event( MapKey( (int)lParam ), false, sys_msg_time);
		return 0;

	case WM_KEYUP:
		if (!g_pKeyboard)
			Key_Event( MapKey( (int)lParam ), false, sys_msg_time);
		break;

#ifdef CD_AUDIO
	case MM_MCINOTIFY:
		{
			LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
			CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
		}
		break;
#endif
	default:	// pass all unhandled messages to DefWindowProc
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
    }

	if (cl_test->intvalue)
		return 0;

    /* return 0 if handled message, 1 if not */
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_ref->modified = true;
	//vid_ref->changed (NULL, NULL, NULL);
	VID_ReloadRefresh ();
}

static void VID_Front_f( void )
{
	SetWindowLong( cl_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST );
	SetForegroundWindow( cl_hwnd );
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	int         width, height;
} vidmode_t;

vidmode_t vid_modes[] =
{
	{320,	240},
	{400,	300},
	{512,	384},
	{640,	480},
	{800,	600},
	{960,	720},
	{1024,	768},
	{1152,	864},
	{1280,	960},
	{1600,	1200},
	{2048,	1536},
	{1280,	1024},
	{1440,	900},
	{1680,	1050},
	{2560,	1920},	
};

qboolean EXPORT VID_GetModeInfo( unsigned int *width, unsigned int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

/*
** VID_UpdateWindowPosAndSize
*/
void VID_UpdateWindowPosAndSize(void)
{
	RECT		r;
	LONG_PTR	style;
	int			w, h;

	r.left   = 0;
	r.top    = 0;
	r.right  = viddef.width;
	r.bottom = viddef.height;

	style = GetWindowLongPtr ( cl_hwnd, GWL_STYLE );
	if (AdjustWindowRect ( &r, (DWORD)style, FALSE ))
	{
		w = r.right - r.left;
		h = r.bottom - r.top;

		MoveWindow( cl_hwnd, vid_xpos->intvalue, vid_ypos->intvalue, w, h, TRUE );
	}
}

/*
** VID_NewWindow
*/
void EXPORT VID_NewWindow ( int width, int height)
{
	//sanity check from renderer
	if (width >= 0x10000 || width < 64 || height >= 0x10000 || height < 48)
		Com_Error (ERR_FATAL, "VID_NewWindow: Illegal width/height %d/%d", width, height);

	viddef.width  = width;
	viddef.height = height;

	//r1: reset input on new window (sw mode mouse fix)
	if (old_hwnd && cl_hwnd != old_hwnd)
		IN_Restart_f ();

	old_hwnd = cl_hwnd;

	cl.force_refdef = true;		// can't use a paused refdef
}

void VID_FreeReflib (void)
{
	if ( !FreeLibrary( reflib_library ) )
		Com_Error( ERR_FATAL, "Reflib FreeLibrary failed" );
	memset (&re, 0, sizeof(re));
	reflib_library = NULL;
	reflib_active  = false;
	cl_hwnd = NULL;
}

//Wrapper function for externally added commands to ensure unclean shutdown
//doesn't trash memory. note that this however LEAKS MEMORY, albeit a tiny bit :)
void EXPORT Cmd_AddCommand_RefCopy (const char *cmd_name, xcommand_t function)
{
	const char *newname;

	newname = CopyString (cmd_name, TAGMALLOC_CMD);
	Cmd_AddCommand (newname, function);
}

DWORD VidDLLExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo, char *errstr)
{
	WNDCLASS				wc;
	CONTEXT					context = *exceptionInfo->ContextRecord;

	if (vid_noexceptionhandler->intvalue)
		return EXCEPTION_CONTINUE_SEARCH;

	//may have left a stale window class after crashing, check
	if (GetClassInfo (global_hInstance, "Quake 2", &wc))
		UnregisterClass ("Quake 2", global_hInstance);

	VID_FreeReflib ();

	sprintf (errstr, "Exception 0x%8x at 0x%8x while initializing", exceptionCode,
#ifdef _M_AMD64
		context.Rip
#else
		context.Eip
#endif
	);

	return EXCEPTION_EXECUTE_HANDLER;
}

/*
==============
VID_LoadRefresh
==============
*/
qboolean VID_LoadRefresh( char *name, char *errstr )
{
	refimport_t		ri;
	refimportnew_t	rx;
	WNDCLASS		wc;
	GetRefAPI_t		GetRefAPI;
	GetExtraAPI_t	GetExtraAPI;
	
	if ( reflib_active )
	{
		closing_reflib = true;
		Com_DPrintf ("closing old reflib.\n");
		re.Shutdown();
		VID_FreeReflib ();
		closing_reflib = false;
	}

	if (!cl_quietstartup->intvalue || developer->intvalue)
		Com_Printf( "------- Loading %s -------\n", LOG_CLIENT, name );

	if ( ( reflib_library = LoadLibrary( name ) ) == 0 )
	{
		int lastError = GetLastError();
		Com_Printf( "LoadLibrary(\"%s\") failed (GetLastError() = %d)\n", LOG_CLIENT, name, lastError);

		if (lastError == 126)
			strcpy (errstr, "file not found");
		else
			sprintf (errstr, "LoadLibrary() failed (%d)", lastError);
		return false;
	}

	Com_DPrintf ("reflib_library initialized.\n");

	ri.Cmd_AddCommand = Cmd_AddCommand_RefCopy;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ExecuteText = Cbuf_ExecuteText;
	ri.Con_Printf = VID_Printf;
	ri.Sys_Error = VID_Error;
	ri.FS_LoadFile = FS_LoadFile;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_Gamedir = FS_Gamedir;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_SetValue = Cvar_SetValue;
	ri.Vid_GetModeInfo = VID_GetModeInfo;
	ri.Vid_MenuInit = VID_MenuInit;
	ri.Vid_NewWindow = VID_NewWindow;

	//EXTENDED FUNCTIONS
	rx.FS_FOpenFile = FS_FOpenFile;
	rx.FS_FCloseFile = FS_FCloseFile;
	rx.FS_Read = FS_Read;

	rx.APIVersion = EXTENDED_API_VERSION;

	Com_DPrintf ("refimport_t set.\n");

	__try
	{
		if ( ( GetRefAPI = (GetRefAPI_t)GetProcAddress( reflib_library, "GetRefAPI" ) ) == 0 )
		{
			VID_FreeReflib ();
			strcpy (errstr, "GetProcAddress() failed");
			//Com_Error( ERR_FATAL, "GetProcAddress failed on %s", name );
			return false;
		}

		Com_DPrintf ("got RefAPI.\n");

		if ( ( GetExtraAPI = (GetExtraAPI_t)GetProcAddress( reflib_library, "GetExtraAPI" ) ) == 0 )
		{
			Com_DPrintf ("No ExtraAPI found.\n");
		}
		else
		{
			Com_DPrintf ("Initializing ExtraAPI...");
			GetExtraAPI (rx);
			Com_DPrintf ("done.\n");
		}

		re = GetRefAPI( ri );

		switch (re.api_version) {
			case 3:
				Com_DPrintf ("api version = %d, ok.\n", re.api_version);
				break;
			default:
				{
					int version = re.api_version;
					Com_DPrintf ("api version = %d, bad.\n", re.api_version);
					VID_FreeReflib ();
					sprintf (errstr, "incompatible api_version %d", version);
					return false;
					//Com_Error (ERR_FATAL, "R1Q2 doesn't support your current renderer (%s).\n\nRequired: API Version 3\nFound: API Version %d", name, version);
				}
		}

		/*if (re.api_version != API_VERSION)
		{
			VID_FreeReflib ();
			Com_Error (ERR_FATAL, "%s has incompatible api_version", name);
		}*/

		if ( re.Init( global_hInstance, MainWndProc ) == -1 )
		{
			Com_DPrintf ("re.Init failed :(\n");
			Com_DPrintf ("gl_driver: %s\n", Cvar_VariableString ("gl_driver"));
			re.Shutdown();
			VID_FreeReflib ();
			strcpy (errstr, "re.Init() failed");

			//may have left a stale window class after crashing, check
			if (GetClassInfo (global_hInstance, "Quake 2", &wc))
				UnregisterClass ("Quake 2", global_hInstance);

			return false;
		}
	}
	__except (VidDLLExceptionHandler (GetExceptionCode (), GetExceptionInformation(), errstr))
	{
		return false;
	}

	if (re.AppActivate == NULL ||
		re.BeginFrame == NULL ||
		re.BeginRegistration == NULL ||
		re.DrawChar == NULL ||
		re.DrawFill == NULL || 
		re.DrawPic == NULL ||
		re.EndFrame == NULL ||
		re.EndRegistration == NULL ||
		re.RenderFrame == NULL ||
		re.Shutdown == NULL)
	{
		strcpy (errstr, "missing exports");
		return false;
	}
		//Com_Error (ERR_FATAL, "Missing export from %s.\n", name);

	Com_DPrintf ("renderer initialized.\n");

	if (!cl_quietstartup->intvalue || developer->intvalue)
		Com_Printf( "------------------------------------\n", LOG_CLIENT);
	reflib_active = true;

	//if (!Sys_CheckFPUStatus())
	//	Com_Printf ("\2WARNING: The FPU control word has changed after loading %s!\n", LOG_GENERAL, name);

//======
//PGM
	vidref_val = VIDREF_OTHER;
	if(vid_ref)
	{
		if(strstr (vid_ref->string, "gl"))
			vidref_val = VIDREF_GL;
		else if(!strcmp(vid_ref->string, "soft"))
			vidref_val = VIDREF_SOFT;
	}
//PGM
//======

	return true;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_ReloadRefresh (void)
{
	char errMessage[256];
	char attempted[512];
	char name[MAX_OSPATH];

	attempted[0] = 0;

	cl.force_refdef = true;		// can't use a paused refdef

	//not needed with openal
	S_StopAllSounds();

	//r1: stop tent models for now.
	CL_ClearTEnts();

	//MessageBox (cl_hwnd, "video restarts!", "what", MB_OK);

	while (vid_ref->modified)
	{
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		//cls.disable_screen = true;

		//r1: nuke local entities
		Le_Reset ();

		//security: prevent loading of external renderer dll that could be supplied by server
		if (strstr (vid_ref->string, "..") || strchr (vid_ref->string, '/') || strchr (vid_ref->string, '\\'))
			Com_Error (ERR_FATAL, "Bad vid_ref '%s'", vid_ref->string);

		Com_sprintf( name, sizeof(name), "ref_%s.dll", vid_ref->string );

		errMessage[0] = 0;

		if (vid_quietload->intvalue && !developer->intvalue)
			ignore_vidprintf = 1;

		if ( !VID_LoadRefresh( name, errMessage ) )
		{
			ignore_vidprintf = 0;
			cl_hwnd = NULL;
			Com_Printf ("\2Failed to load %s: %s\n", LOG_GENERAL, name, errMessage);
			if (attempted[0])
				strcat (attempted, "\n");
			strcat (attempted, name);
			strcat (attempted, " (");
			strcat (attempted, errMessage);
			strcat (attempted, ")");

			if ( strcmp (vid_ref->string, "soft") == 0 )
				Com_Error (ERR_FATAL, 	"Unable to initialize video output! Please check that your video card drivers are installed and up to date and that Quake II is installed properly. If you continue to get this error, try using R1GL from http://www.r1ch.net/stuff/r1gl/ and ensure your gl_driver is set to 'opengl32' in your autoexec.cfg\n\n"
										"The following renderer DLLs failed to load:\n\n"
										"%s",
										attempted);
			Cvar_Set( "vid_ref", "soft" );

			/*
			** drop the console if we fail to load a refresh
			*/
			if ( cls.key_dest != key_console )
			{
				Con_ToggleConsole_f();
			}
		}

		ignore_vidprintf = 0;

		{
			HICON icon;
			icon = LoadIcon (global_hInstance, (MAKEINTRESOURCE(IDI_ICON1)));
			SendMessage (cl_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
			SendMessage (cl_hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
		}

		//r1ch: restart our sound/input devices as the window handle most likely changed
		if (old_hwnd && cl_hwnd != old_hwnd)
			IN_Restart_f ();

		old_hwnd = cl_hwnd;
		//cls.disable_screen = false;
		Con_CheckResize();
	}
}

void VID_AltTab_Modified (cvar_t *cvar, char *old, char *newv)
{
	if ( cvar->intvalue )
	{
		WIN_DisableAltTab();
	}
	else
	{
		WIN_EnableAltTab();
	}
	cvar->modified = false;
}

void VID_XY_Modified (cvar_t *cvar, char *old, char *newv)
{
	if (!vid_fullscreen->intvalue)
		VID_UpdateWindowPosAndSize();

	vid_xpos->modified = false;
	vid_ypos->modified = false;
}

HHOOK		g_hKeyboardHook;

cvar_t		*win_disablewinkey;

//FIXME: Nt4+ only :E
LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
	qboolean bEatKeystroke;
	qboolean down;

    if (nCode < 0 || nCode != HC_ACTION )  // do not process message 
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam); 
 
	bEatKeystroke = false;
	down = false;
    
    switch (wParam) 
    {
        case WM_KEYDOWN:
			down = true;
        case WM_KEYUP:    
        {
            bEatKeystroke = (ActiveApp && ((p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN)));
            break;
        }
    }
 
    if( bEatKeystroke )
	{
		int	key;

		if (p->vkCode == VK_LWIN)
			key = K_LWINKEY;
		else
			key = K_RWINKEY;

		Key_Event (key, down, ((KBDLLHOOKSTRUCT *)lParam)->time);
        return 1;
	}
    else
	{
        return CallNextHookEx( g_hKeyboardHook, nCode, wParam, lParam );
	}
}

void _winkey_changed (cvar_t *cvar, char *old, char *newv)
{
	if (!os_winxp && cvar->intvalue)
	{
		Com_Printf ("win_disablewinkey requires Windows 2000 or higher.\n", LOG_CLIENT);
		Cvar_Set ("win_disablewinkey", "0");
		return;
	}

	if (!g_hKeyboardHook && cvar->intvalue)
	{
#define WH_KEYBOARD_LL 13
		g_hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(NULL), 0 );
		if (!g_hKeyboardHook)
		{
			Com_Printf ("Unable to install KeyboardHook for some reason :(\n", LOG_CLIENT);
			Cvar_Set ("win_disablewinkey", "0");
		}
		else
		{
			Com_Printf ("KeyboardHook installed - Windows key disabled.\n", LOG_CLIENT);
		}
	}
	else if (g_hKeyboardHook && !cvar->intvalue)
	{
		Com_Printf ("KeyboardHook removed - Windows key enabled.\n", LOG_CLIENT);
		UnhookWindowsHookEx (g_hKeyboardHook);
		g_hKeyboardHook = NULL;
	}
}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get ("vid_ref", "gl", CVAR_ARCHIVE);
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );
	vid_quietload = Cvar_Get ("vid_quietload", "1", 0);
	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );
	win_disablewinkey = Cvar_Get ("win_disablewinkey", "0", 0);
	//win_nopriority = Cvar_Get ("win_nopriority", "0", CVAR_ARCHIVE);

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_front", VID_Front_f);

	//r1q2 specific:
	vid_xpos->changed = VID_XY_Modified;
	vid_ypos->changed = VID_XY_Modified;
	//vid_fullscreen->changed = VID_Ref_Modified;
	//vid_ref->changed = VID_Ref_Modified;

	//FIXME: make this changable
	/*if (gl_mode)
		gl_mode->changed = VID_Ref_Modified;*/
	
	win_noalttab->changed = VID_AltTab_Modified;
	win_noalttab->changed (win_noalttab, win_noalttab->string, win_noalttab->string);

	vid_noexceptionhandler = Cvar_Get ("vid_noexceptionhandler", "1", 0);

	/*
	** this is a gross hack but necessary to clamp the mode for 3Dfx
	*/
#if 0
	{
		cvar_t *gl_driver = Cvar_Get( "gl_driver", "opengl32", 0 );
		cvar_t *gl_mode = Cvar_Get( "gl_mode", "3", 0 );

		if ( Q_stricmp( gl_driver->string, "3dfxgl" ) == 0 )
		{
			Cvar_Set( "gl_mode", "3" );
			viddef.width  = 640;
			viddef.height = 480;
		}
	}
#endif

	/* Disable the 3Dfx splash screen */
	putenv("FX_GLIDE_NO_SPLASH=0");
		
	/* Start the graphics mode and load refresh DLL */
	//vid_ref->changed (NULL, NULL, NULL);
	VID_ReloadRefresh ();

	win_disablewinkey->changed = _winkey_changed;
	_winkey_changed (win_disablewinkey, win_disablewinkey->string, win_disablewinkey->string);

	//FIXME: combine multi line changes into a single reload of refresh dll
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if ( reflib_active )
	{
		re.Shutdown ();
		VID_FreeReflib ();
	}

	if (g_hKeyboardHook)
	{
		UnhookWindowsHookEx (g_hKeyboardHook);
		g_hKeyboardHook = NULL;
	}
}


