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
// in_win.c -- windows 95 mouse and joystick code
// 02/21/97 JCB Added extended DirectInput code to support external controllers.

#include "../client/client.h"
#include "winquake.h"

extern	unsigned	sys_msg_time;

cvar_t	*in_mouse;
cvar_t	*m_directinput;

#ifdef JOYSTICK
// joystick defines and variables
// where should defines be moved?
#define JOY_ABSOLUTE_AXIS	0x00000000		// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010		// control like a mouse, spinner, trackball
#define	JOY_MAX_AXES		6				// X, Y, Z, R, U, V
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList
{
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn, AxisUp
};

DWORD	dwAxisFlags[JOY_MAX_AXES] =
{
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU, JOY_RETURNV
};

DWORD	dwAxisMap[JOY_MAX_AXES];
DWORD	dwControlMap[JOY_MAX_AXES];
PDWORD	pdwRawValue[JOY_MAX_AXES];

cvar_t	*in_joystick;


// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage
// or when changing from one controller to another.  this way at least something
// works.
cvar_t	*joy_name;
cvar_t	*joy_advanced;
cvar_t	*joy_advaxisx;
cvar_t	*joy_advaxisy;
cvar_t	*joy_advaxisz;
cvar_t	*joy_advaxisr;
cvar_t	*joy_advaxisu;
cvar_t	*joy_advaxisv;
cvar_t	*joy_forwardthreshold;
cvar_t	*joy_sidethreshold;
cvar_t	*joy_pitchthreshold;
cvar_t	*joy_yawthreshold;
cvar_t	*joy_forwardsensitivity;
cvar_t	*joy_sidesensitivity;
cvar_t	*joy_pitchsensitivity;
cvar_t	*joy_yawsensitivity;
cvar_t	*joy_upthreshold;
cvar_t	*joy_upsensitivity;

qboolean	joy_avail, joy_advancedinit, joy_haspov;
DWORD		joy_oldbuttonstate, joy_oldpovstate;

int			joy_id;
DWORD		joy_flags;
DWORD		joy_numbuttons;

static JOYINFOEX	ji;

// forward-referenced functions
void IN_StartupJoystick (void);
void Joy_AdvancedUpdate_f (void);
void IN_JoyMove (usercmd_t *cmd);
#endif

qboolean	in_appactive;

/*
============================================================

  MOUSE CONTROL

============================================================
*/

// mouse variables
cvar_t	*m_filter;
cvar_t	*m_winxp_fix;
cvar_t	*m_show;

qboolean	mlooking;

void IN_MLookDown (void) { mlooking = true; }
void IN_MLookUp (void) {
mlooking = false;
if (!freelook->value && lookspring->value)
		IN_CenterView ();
}

int			mouse_buttons;
int			mouse_oldbuttonstate;
POINT		current_pos;
int			mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

int			old_x, old_y;

qboolean	mouseactive;	// false when not focus app

qboolean	restore_spi;
qboolean	mouseinitialized;
int		originalmouseparms[3], newmouseparms[3] = {0, 0, 1}, winxpmouseparms[3] = {0, 0, 0};
qboolean	mouseparmsvalid;

int			window_center_x, window_center_y;
RECT		window_rect;

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
#ifdef DIRECTINPUT_MOUSE_SUPPORT
LPDIRECTINPUT8       g_pDI    = NULL;   
LPDIRECTINPUTDEVICE8 g_pMouse = NULL;    

//-----------------------------------------------------------------------------
// Name: FreeDirectInput()
// Desc: Initialize the DirectInput variables.
//-----------------------------------------------------------------------------
void FreeDirectInput(int totalShutdown)
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
	if( g_pMouse ) {
        //g_pMouse->Unacquire();
		//Com_Printf ("Unacquire() free\n");
		IDirectInputDevice8_Unacquire (g_pMouse);
	}
    
    // Release any DirectInput objects.
	if( g_pMouse && totalShutdown) {
		IDirectInputDevice8_Release (g_pMouse);
		g_pMouse = NULL;
	}

	if (g_pDI && totalShutdown) {
		IDirectInput8_Release (g_pDI);
		g_pDI = NULL;
	}
}

DIMOUSESTATE2 old_state;

int IN_InitDInputMouse (void)
{
    HRESULT hr;
    BOOL    bExclusive;
    BOOL    bForeground;
    BOOL    bImmediate;
    DWORD   dwCoopFlags;

    // Cleanup any previous call first
    //KillTimer( cl_hwnd, 0 );    
    FreeDirectInput(1);

    // Detrimine where the buffer would like to be allocated 
    bExclusive         = 1;//( IsDlgButtonChecked( hDlg, IDC_EXCLUSIVE  ) == BST_CHECKED );
    bForeground        = 1;//( IsDlgButtonChecked( hDlg, IDC_FOREGROUND ) == BST_CHECKED );
    bImmediate         = 0;//( IsDlgButtonChecked( hDlg, IDC_IMMEDIATE  ) == BST_CHECKED );

    if( bExclusive )
        dwCoopFlags = DISCL_EXCLUSIVE;
    else
        dwCoopFlags = DISCL_NONEXCLUSIVE;

    if( bForeground )
        dwCoopFlags |= DISCL_FOREGROUND;
    else
        dwCoopFlags |= DISCL_BACKGROUND;

    // Create a DInput object
	Com_Printf ("...creating DirectInput object: ");
	
	//extern HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter);
	//DirectInput8Create(hTheInstance, DIRECTINPUT_VERSION, , NULL);
    if( FAILED( hr = DirectInput8Create( global_hInstance, DIRECTINPUT_VERSION, (const GUID *)&IID_IDirectInput8,
                                         (VOID**)&g_pDI, NULL ) ) )
    {
		Com_Printf ("failed: %d\n", hr);
        return 1;
    }
	Com_Printf ("ok\n");
    
    // Obtain an interface to the system mouse device.
	Com_Printf ("...creating device interface: ");
    if( FAILED( hr = IDirectInput8_CreateDevice(g_pDI, (const GUID *)&GUID_SysMouse, &g_pMouse, NULL ) ) )
    {
		Com_Printf ("failed: %d\n", hr);
        return 1;
    }
	Com_Printf ("ok\n");
    
    // Set the data format to "mouse format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing a
    // DIMOUSESTATE2 structure to IDirectInputDevice::GetDeviceState.
	Com_Printf ("...setting data format: ");
	if( FAILED( hr = IDirectInputDevice8_SetDataFormat(g_pMouse, &c_dfDIMouse2 ) ) )
    {
		Com_Printf ("failed: %d\n", hr);
        return 1;
    }
	Com_Printf ("ok\n");

    // Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications.
	Com_Printf ("...setting DISCL_EXCLUSIVE coop level: ");
    hr = IDirectInputDevice8_SetCooperativeLevel (g_pMouse, cl_hwnd, dwCoopFlags );
    if( hr == DIERR_UNSUPPORTED && !bForeground && bExclusive )
    {
        FreeDirectInput(1);
		Com_Printf ("failed: DIERR_UNSUPPORTED\n");
        return 1;
    } else if (FAILED (hr)) {
		Com_Printf ("failed: %d\n", hr);
        return 1;
    }
	Com_Printf ("ok\n");

    if( !bImmediate )
    {
        // IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
        //
        // DirectInput uses unbuffered I/O (buffer size = 0) by default.
        // If you want to read buffered data, you need to set a nonzero
        // buffer size.
        //
        // Set the buffer size to SAMPLE_BUFFER_SIZE (defined above) elements.
        //
        // The buffer size is a DWORD property associated with the device.
        DIPROPDWORD dipdw;
        dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj        = 0;
        dipdw.diph.dwHow        = DIPH_DEVICE;
        dipdw.dwData            = 16; // Arbitary buffer size

        if( FAILED( hr = IDirectInputDevice8_SetProperty(g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph ) ) )
            return hr;
    }

    // Acquire the newly created device
    //g_pMouse->Acquire();
	Com_Printf ("...Acquire()ing device... ");
	IDirectInputDevice8_Acquire (g_pMouse);
	Com_Printf ("ok\n");

	memset (&old_state, 0, sizeof(old_state));


    // Set a timer to go off 12 times a second, to read input
    // Note: Typically an application would poll the mouse
    //       much faster than this, but this slow rate is simply 
    //       for the purposes of demonstration
    //SetTimer( cl_hwnd, 0, 1, NULL );

    return 0;
}
#define MAX_MOUSE_BUTTONS 8

int IN_ReadImmediateData (usercmd_t *cmd)
{
	int i;
	float		mx, my;
    HRESULT       hr;
    DIMOUSESTATE2 dims2;      // DirectInput mouse state structure

    if( NULL == g_pMouse ) 
        return S_OK;
    
    // Get the input's device state, and put the state in dims
    memset(&dims2, 0, sizeof(dims2));
    hr = IDirectInputDevice8_GetDeviceState(g_pMouse, sizeof(DIMOUSESTATE2), &dims2 );
    if( FAILED(hr) ) 
    {
        // DirectInput may be telling us that the input stream has been
        // interrupted.  We aren't tracking any state between polls, so
        // we don't have any special reset that needs to be done.
        // We just re-acquire and try again.
		Com_Printf ("Lost Mouse!\n");
        
        // If input is lost then acquire and keep trying 
		//Com_Printf ("Acquire() from readdata\n");
		hr = IDirectInputDevice8_Acquire (g_pMouse);
        //hr = g_pMouse->Acquire();
		while( hr == DIERR_INPUTLOST )  {
            //hr = g_pMouse->Acquire();
			Com_Printf ("Acquire() from readdata\n");
			hr = IDirectInputDevice8_Acquire (g_pMouse);
		}
        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
        return S_OK; 
    }
    
    // The dims structure now has the state of the mouse, so 
    // display mouse coordinates (x, y, z) and buttons.
	if (m_show->value) {
    Com_Printf ("(X=% 3.3d, Y=% 3.3d, Z=% 3.3d) B0=%c B1=%c B2=%c B3=%c B4=%c B5=%c B6=%c B7=%c\n",
                         dims2.lX, dims2.lY, dims2.lZ,
                        (dims2.rgbButtons[0] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[1] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[2] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[3] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[4] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[5] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[6] & 0x80) ? '1' : '0',
                        (dims2.rgbButtons[7] & 0x80) ? '1' : '0');
	}

	for (i = 0; i < MAX_MOUSE_BUTTONS; i++) {
		if (old_state.rgbButtons[i] & 0x80 && !(dims2.rgbButtons[i] & 0x80)) {
			Key_Event (K_MOUSE1 + i, false, 10);
		} else if (dims2.rgbButtons[i] & 0x80 && !(old_state.rgbButtons[i] & 0x80)) {
			Key_Event (K_MOUSE1 + i, true, 10);
		}
	}

	if (dims2.lZ > 0) {
		Key_Event (K_MWHEELUP, true, 10);
		Key_Event (K_MWHEELUP, false, 10);
	} else if (dims2.lZ < 0) {
		Key_Event (K_MWHEELDOWN, true, 10);
		Key_Event (K_MWHEELDOWN, false, 10);
	}

	memcpy (&old_state, &dims2, sizeof(old_state));

	mx = dims2.lX;
	my = dims2.lY;

	mx *= sensitivity->value;
	my *= sensitivity->value;

	// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking )) {
		if (cmd)
			cmd->sidemove += m_side->value * mx;
	} else {
		cl.viewangles[YAW] -= m_yaw->value * mx;
	}

	if ( (mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * my;
	}
	else
	{
		if (cmd)
			cmd->forwardmove -= m_forward->value * my;
	}

	return 0;
}
#endif

/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse (void)
{
	int		width, height;

	if (!mouseinitialized)
		return;

	if (!in_mouse->value)
	{
		mouseactive = false;
		return;
	}

	if (mouseactive)
		return;

	//Com_Printf ("******************* IN_ActivateMouse\n");

#ifdef DIRECTINPUT_MOUSE_SUPPORT
	if (g_pDI) {
		//Com_Printf ("Acquire() from activatemouse\n");
		IDirectInputDevice8_Acquire (g_pMouse);
	} else {
#endif
		if (mouseparmsvalid) {
			if (m_winxp_fix->value)
				restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, winxpmouseparms, 0);
			else
				restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);
		}

		width = GetSystemMetrics (SM_CXSCREEN);
		height = GetSystemMetrics (SM_CYSCREEN);

		GetWindowRect ( cl_hwnd, &window_rect);
		if (window_rect.left < 0)
			window_rect.left = 0;
		if (window_rect.top < 0)
			window_rect.top = 0;
		if (window_rect.right >= width)
			window_rect.right = width-1;
		if (window_rect.bottom >= height-1)
			window_rect.bottom = height-1;

		window_center_x = (window_rect.right + window_rect.left)/2;
		window_center_y = (window_rect.top + window_rect.bottom)/2;

		SetCursorPos (window_center_x, window_center_y);

		old_x = window_center_x;
		old_y = window_center_y;

		SetCapture ( cl_hwnd );
		ClipCursor (&window_rect);
#ifdef DIRECTINPUT_MOUSE_SUPPORT
	}
#endif

	mouseactive = true;

	while (ShowCursor (FALSE) >= 0)
		;
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse (void)
{
	if (!mouseinitialized)
		return;

	if (!mouseactive)
		return;

	//Com_Printf (" ************** IN_DeactivateMouse ***************\n");

#ifdef DIRECTINPUT_MOUSE_SUPPORT
	if (g_pDI) {
		FreeDirectInput(0);
	} else {
#endif
		if (restore_spi)
			SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

		ClipCursor (NULL);
		ReleaseCapture ();	
#ifdef DIRECTINPUT_MOUSE_SUPPORT
	}
#endif

	mouseactive = false;
	while (ShowCursor (TRUE) < 0);
}

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse (void)
{
	//cvar_t		*cv;

	if (!in_mouse->value)
		return;

#ifdef DIRECTINPUT_MOUSE_SUPPORT
	if (m_directinput->value)
	{
		if (IN_InitDInputMouse())
		{
			Com_Printf ("Falling back to standard mouse support.\n");
			Cvar_ForceSet ("m_directinput", "0");
		}
	}
#endif

	/*cv = Cvar_Get ("in_initmouse", "1", CVAR_NOSET);
	if ( !cv->value ) 
		return; */

	mouseinitialized = true;
	mouseparmsvalid = SystemParametersInfo (SPI_GETMOUSE, 0, originalmouseparms, 0);
	mouse_buttons = 8;
}

void IN_Restart_f (void)
{
	IN_Shutdown ();

#ifdef JOYSTICK
	IN_StartupJoystick ();
#endif
	IN_StartupMouse ();
}

/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate)
{
	int		i;

	if (!mouseinitialized
#ifdef DIRECTINPUT_MOUSE_SUPPORT
		|| g_pDI
#endif
		)
		return;

// perform button actions
	for (i=0 ; i<mouse_buttons ; i++)
	{
		if ( (mstate & (1<<i)) &&
			!(mouse_oldbuttonstate & (1<<i)) )
		{
			Key_Event (K_MOUSE1 + i, true, sys_msg_time);
		}

		if ( !(mstate & (1<<i)) &&
			(mouse_oldbuttonstate & (1<<i)) )
		{
				Key_Event (K_MOUSE1 + i, false, sys_msg_time);
		}
	}	
		
	mouse_oldbuttonstate = mstate;
}


/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove (usercmd_t *cmd)
{
	int		mx, my;

	if (!mouseactive)
		return;

#ifdef DIRECTINPUT_MOUSE_SUPPORT
	if (g_pDI) {
		IN_ReadImmediateData (cmd);
		return;
	}
#endif

	// find mouse movement
	if (!GetCursorPos (&current_pos))
		return;

	mx = current_pos.x - window_center_x;
	my = current_pos.y - window_center_y;

#if 0
	if (!mx && !my)
		return;
#endif

	if (m_filter->value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ( (mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	}
	else
	{
		cmd->forwardmove -= m_forward->value * mouse_y;
	}

	// force the mouse to the center, so there's room to move
	if (mx || my) {
		//Com_Printf ("******** SETCURSORPOS\n");
		SetCursorPos (window_center_x, window_center_y);
	}
}


/*
=========================================================================

VIEW CENTERING

=========================================================================
*/

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;


/*
===========
IN_Init
===========
*/
void IN_Init (void)
{
	// mouse variables
	m_filter				= Cvar_Get ("m_filter",					"0",		0);
	m_winxp_fix				= Cvar_Get ("m_fixaccel",				"0",		0);
	m_show					= Cvar_Get ("m_show",					"0",		0);
	m_directinput			= Cvar_Get ("m_directinput",			"0",		0);
    in_mouse				= Cvar_Get ("in_mouse",					"1",		0);

#ifdef JOYSTICK
	// joystick variables
	in_joystick				= Cvar_Get ("in_joystick",				"0",		CVAR_ARCHIVE);
	joy_name				= Cvar_Get ("joy_name",					"joystick",	0);
	joy_advanced			= Cvar_Get ("joy_advanced",				"0",		0);
	joy_advaxisx			= Cvar_Get ("joy_advaxisx",				"0",		0);
	joy_advaxisy			= Cvar_Get ("joy_advaxisy",				"0",		0);
	joy_advaxisz			= Cvar_Get ("joy_advaxisz",				"0",		0);
	joy_advaxisr			= Cvar_Get ("joy_advaxisr",				"0",		0);
	joy_advaxisu			= Cvar_Get ("joy_advaxisu",				"0",		0);
	joy_advaxisv			= Cvar_Get ("joy_advaxisv",				"0",		0);
	joy_forwardthreshold	= Cvar_Get ("joy_forwardthreshold",		"0.15",		0);
	joy_sidethreshold		= Cvar_Get ("joy_sidethreshold",		"0.15",		0);
	joy_upthreshold  		= Cvar_Get ("joy_upthreshold",			"0.15",		0);
	joy_pitchthreshold		= Cvar_Get ("joy_pitchthreshold",		"0.15",		0);
	joy_yawthreshold		= Cvar_Get ("joy_yawthreshold",			"0.15",		0);
	joy_forwardsensitivity	= Cvar_Get ("joy_forwardsensitivity",	"-1",		0);
	joy_sidesensitivity		= Cvar_Get ("joy_sidesensitivity",		"-1",		0);
	joy_upsensitivity		= Cvar_Get ("joy_upsensitivity",		"-1",		0);
	joy_pitchsensitivity	= Cvar_Get ("joy_pitchsensitivity",		"1",		0);
	joy_yawsensitivity		= Cvar_Get ("joy_yawsensitivity",		"-1",		0);
#endif

	// centering
	v_centermove			= Cvar_Get ("v_centermove",				"0.15",		0);
	v_centerspeed			= Cvar_Get ("v_centerspeed",			"500",		0);

	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	Cmd_AddCommand ("in_restart", IN_Restart_f);

#ifdef JOYSTICK
	Cmd_AddCommand ("joy_advancedupdate", Joy_AdvancedUpdate_f);
	IN_StartupJoystick ();
#endif

	IN_StartupMouse ();
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown (void)
{
	IN_DeactivateMouse ();
#ifdef DIRECTINPUT_MOUSE_SUPPORT
	FreeDirectInput(1);
#endif
}


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate (qboolean active)
{
	//Com_Printf ("*************** IN_Activate ACTIVE = %d\n\n\n", active);
	in_appactive = active;

	if (!active)
		IN_DeactivateMouse ();
	//mouseactive = active;		// force a new window check or turn off
	//Com_Printf ("mouseactive = %d\n", mouseactive);
}


/*
==================
IN_Frame

Called every frame, even if not generating commands
==================
*/
void IN_Frame (void)
{
	if (!mouseinitialized)
		return;

	if (!in_mouse || !in_appactive)
	{
		IN_DeactivateMouse ();
		return;
	}

	if ( !cl.refresh_prepped
		|| cls.key_dest == key_console
		|| cls.key_dest == key_menu)
	{
		// temporarily deactivate if in fullscreen
		if (Cvar_VariableValue ("vid_fullscreen") == 0)
		{
			IN_DeactivateMouse ();
			return;
		}
	}

	IN_ActivateMouse ();
}

/*
===========
IN_Move
===========
*/
void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove (cmd);

#ifdef JOYSTICK
	if (ActiveApp)
		IN_JoyMove (cmd);
#endif
}


/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{
	mx_accum = 0;
	my_accum = 0;
	mouse_oldbuttonstate = 0;
}


/*
=========================================================================

JOYSTICK

=========================================================================
*/

/* 
=============== 
IN_StartupJoystick 
=============== 
*/  
#ifdef JOYSTICK
void IN_StartupJoystick (void) 
{ 
	int			numdevs;
	JOYCAPS		jc;
	MMRESULT	mmr;
	cvar_t		*cv;

 	// assume no joystick
	joy_avail = false; 

	// abort startup if user requests no joystick
	cv = Cvar_Get ("in_initjoy", "1", CVAR_NOSET);
	if ( !cv->value ) 
		return; 
 
	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0)
	{
//		Com_Printf ("\njoystick not found -- driver not present\n\n");
		return;
	}

	// cycle through the joystick ids for the first valid one
	for (joy_id=0 ; joy_id<numdevs ; joy_id++)
	{
		memset (&ji, 0, sizeof(ji));
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	} 

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Com_Printf ("\njoystick not found -- no valid joysticks (%x)\n\n", mmr);
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof(jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof(jc))) != JOYERR_NOERROR)
	{
		Com_Printf ("\njoystick not found -- invalid joystick capabilities (%x)\n\n", mmr); 
		return;
	}

	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not completed
	// this is needed as cvars are not available during initialization

	joy_avail = true; 
	joy_advancedinit = false;

	Com_Printf ("\njoystick detected\n\n"); 
}


/*
===========
RawValuePointer
===========
*/
PDWORD RawValuePointer (int axis)
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return &ji.dwXpos;
	case JOY_AXIS_Y:
		return &ji.dwYpos;
	case JOY_AXIS_Z:
		return &ji.dwZpos;
	case JOY_AXIS_R:
		return &ji.dwRpos;
	case JOY_AXIS_U:
		return &ji.dwUpos;
	case JOY_AXIS_V:
		return &ji.dwVpos;
	}

	//shut up
	return NULL;
}


/*
===========
Joy_AdvancedUpdate_f
===========
*/
void Joy_AdvancedUpdate_f (void)
{

	// called once by IN_ReadJoystick and by user whenever an update is needed
	// cvars are now available
	int	i;
	DWORD dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer(i);
	}

	if( joy_advanced->value == 0.0)
	{
		// default joystick initialization
		// 2 axes only with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	}
	else
	{
		if (strcmp (joy_name->string, "joystick") != 0)
		{
			// notify user of advanced controller
			Com_Printf ("\n%s configured\n\n", joy_name->string);
		}

		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = (DWORD) joy_advaxisx->value;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisy->value;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisz->value;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisr->value;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisu->value;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = (DWORD) joy_advaxisv->value;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		if (dwAxisMap[i] != AxisNada)
		{
			joy_flags |= dwAxisFlags[i];
		}
	}
}

/*
===========
IN_Commands
===========
*/
#ifdef JOYSTICK
void IN_Commands (void)
{
	int		i, key_index;
	DWORD	buttonstate, povstate;

	if (!joy_avail)
	{
		return;
	}

	
	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = ji.dwButtons;
	for (i=0 ; i < joy_numbuttons ; i++)
	{
		if ( (buttonstate & (1<<i)) && !(joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, true, 0);
		}

		if ( !(buttonstate & (1<<i)) && (joy_oldbuttonstate & (1<<i)) )
		{
			key_index = (i < 4) ? K_JOY1 : K_AUX1;
			Key_Event (key_index + i, false, 0);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov)
	{
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if(ji.dwPOV != JOY_POVCENTERED)
		{
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for each change
		for (i=0 ; i < 4 ; i++)
		{
			if ( (povstate & (1<<i)) && !(joy_oldpovstate & (1<<i)) )
			{
				Key_Event (K_AUX29 + i, true, 0);
			}

			if ( !(povstate & (1<<i)) && (joy_oldpovstate & (1<<i)) )
			{
				Key_Event (K_AUX29 + i, false, 0);
			}
		}
		joy_oldpovstate = povstate;
	}
}
#endif
/* 
=============== 
IN_ReadJoystick
=============== 
*/  
qboolean IN_ReadJoystick (void)
{

	memset (&ji, 0, sizeof(ji));
	ji.dwSize = sizeof(ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR)
	{
		return true;
	}
	else
	{
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,\
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy_avail = false;
		return false;
	}
}


/*
===========
IN_JoyMove
===========
*/
void IN_JoyMove (usercmd_t *cmd)
{
	float	speed, aspeed;
	float	fAxisValue;
	int		i;

	// complete initialization if first time in
	// this is needed as cvars are not available at initialization time
	if( joy_advancedinit != true )
	{
		Joy_AdvancedUpdate_f();
		joy_advancedinit = true;
	}

	// verify joystick is available and that the user wants to use it
	if (!joy_avail || !in_joystick->value)
	{
		return; 
	}
 
	// collect the joystick data, if possible
	if (IN_ReadJoystick () != true)
	{
		return;
	}

	if ( (in_speed.state & 1) ^ (int)cl_run->value)
		speed = 2;
	else
		speed = 1;
	aspeed = speed * cls.frametime;

	// loop through the axes
	for (i = 0; i < JOY_MAX_AXES; i++)
	{
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = (float) *pdwRawValue[i];
		// move centerpoint to zero
		fAxisValue -= 32768.0;

		// convert range from -32768..32767 to -1..1 
		fAxisValue /= 32768.0;

		switch (dwAxisMap[i])
		{
		case AxisForward:
			if ((joy_advanced->value == 0.0) && mlooking)
			{
				// user wants forward control to become look control
				if (fabs(fAxisValue) > joy_pitchthreshold->value)
				{		
					// if mouse invert is on, invert the joystick pitch value
					// only absolute control support here (joy_advanced is false)
					if (m_pitch->value < 0.0)
					{
						cl.viewangles[PITCH] -= (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
				}
			}
			else
			{
				// user wants forward control to be forward control
				if (fabs(fAxisValue) > joy_forwardthreshold->value)
				{
					cmd->forwardmove += (fAxisValue * joy_forwardsensitivity->value) * speed * cl_forwardspeed->value;
				}
			}
			break;

		case AxisSide:
			if (fabs(fAxisValue) > joy_sidethreshold->value)
			{
				cmd->sidemove += (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
			}
			break;

		case AxisUp:
			if (fabs(fAxisValue) > joy_upthreshold->value)
			{
				cmd->upmove += (fAxisValue * joy_upsensitivity->value) * speed * cl_upspeed->value;
			}
			break;

		case AxisTurn:
			if ((in_strafe.state & 1) || (lookstrafe->value && mlooking))
			{
				// user wants turn control to become side control
				if (fabs(fAxisValue) > joy_sidethreshold->value)
				{
					cmd->sidemove -= (fAxisValue * joy_sidesensitivity->value) * speed * cl_sidespeed->value;
				}
			}
			else
			{
				// user wants turn control to be turn control
				if (fabs(fAxisValue) > joy_yawthreshold->value)
				{
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * aspeed * cl_yawspeed->value;
					}
					else
					{
						cl.viewangles[YAW] += (fAxisValue * joy_yawsensitivity->value) * speed * 180.0;
					}

				}
			}
			break;

		case AxisLook:
			if (mlooking)
			{
				if (fabs(fAxisValue) > joy_pitchthreshold->value)
				{
					// pitch movement detected and pitch movement desired by user
					if(dwControlMap[i] == JOY_ABSOLUTE_AXIS)
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;
					}
					else
					{
						cl.viewangles[PITCH] += (fAxisValue * joy_pitchsensitivity->value) * speed * 180.0;
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

#endif
