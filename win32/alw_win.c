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

#ifdef USE_OPENAL

#include "../qcommon/qcommon.h"
#include "winquake.h"
#include "../client/snd_loc.h"


alConfig_t	alConfig;
alwState_t	alwState;


/*
 =================
 ALW_InitExtensions
 =================
*/
static void ALW_InitExtensions (void)
{
	if (!s_openal_extensions->intvalue)
	{
		Com_Printf("*** IGNORING OPENAL EXTENSIONS ***\n", LOG_CLIENT);
		return;
	}

	Com_Printf("Initializing OpenAL extensions\n", LOG_CLIENT);

	if (qalIsExtensionPresent("EAX2.0"))
	{
		if (s_openal_eax->intvalue)
		{
			alConfig.eax = true;

			qalEAXSet = (ALEAXSET)qalGetProcAddress("EAXSet");
			qalEAXGet = (ALEAXGET)qalGetProcAddress("EAXGet");

			Com_Printf("...using EAX2.0\n", LOG_CLIENT);
		}
		else
			Com_Printf("...ignoring EAX2.0\n", LOG_CLIENT);
	}
	else
		Com_Printf("...EAX2.0 not found\n", LOG_CLIENT);
}

/*
 =================
 ALW_InitDriver
 =================
*/
static qboolean ALW_InitDriver (void)
{
	char	*deviceName;

	Com_Printf("Initializing OpenAL driver\n", LOG_CLIENT);

	// Open the device
	deviceName = s_openal_device->string;

	if (!deviceName[0])
		deviceName = NULL;

	if (deviceName)
		Com_Printf("...opening device (%s): ", LOG_CLIENT, deviceName);
	else
		Com_Printf("...opening device: ", LOG_CLIENT);

	if ((alwState.hDevice = qalcOpenDevice(deviceName)) == NULL)
	{
		Com_Printf("failed\n", LOG_CLIENT);
		return false;
	}

	if (!deviceName)
		Com_Printf("succeeded (%s)\n", LOG_CLIENT, qalcGetString(alwState.hDevice, ALC_DEVICE_SPECIFIER));
	else
		Com_Printf("succeeded\n", LOG_CLIENT);

	// Create the AL context and make it current
	Com_Printf("...creating AL context: ", LOG_CLIENT);
	if ((alwState.hALC = qalcCreateContext(alwState.hDevice, NULL)) == NULL)
	{
		Com_Printf("failed\n", LOG_CLIENT);
		goto failed;
	}
	Com_Printf("succeeded\n", LOG_CLIENT);

	Com_Printf("...making context current: ", LOG_CLIENT);
	if (!qalcMakeContextCurrent(alwState.hALC))
	{
		Com_Printf("failed\n", LOG_CLIENT);
		goto failed;
	}
	Com_Printf("succeeded\n", LOG_CLIENT);

	return true;

failed:

	Com_Printf("...failed hard\n", LOG_CLIENT);

	if (alwState.hALC)
	{
		qalcDestroyContext(alwState.hALC);
		alwState.hALC = NULL;
	}

	if (alwState.hDevice)
	{
		qalcCloseDevice(alwState.hDevice);
		alwState.hDevice = NULL;
	}

	return false;
}

/*
 =================
 ALW_StartOpenAL
 =================
*/
static qboolean ALW_StartOpenAL (const char *driver)
{
	// Initialize our QAL dynamic bindings
	if (!QAL_Init(driver))
		return false;

	// Get device list
	if (qalcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT"))
		alConfig.deviceList = qalcGetString(NULL, ALC_DEVICE_SPECIFIER);
	else
		alConfig.deviceList = "DirectSound3D\0DirectSound\0MMSYSTEM\0\0";

	// Initialize the device, context, etc...
	if (ALW_InitDriver())
		return true;

	// Shutdown QAL
	QAL_Shutdown();

	return false;
}

/*
 =================
 ALW_Init
 =================
*/
qboolean ALW_Init (void){

	Com_Printf("Initializing OpenAL subsystem\n", LOG_CLIENT);

	// Initialize OpenAL subsystem
	if (!ALW_StartOpenAL("OpenAL32"))
	{
		// Let the user continue without sound
		Com_Printf ("WARNING: OpenAL initialization failed\n", LOG_CLIENT|LOG_WARNING);
		return false;
	}

	// Get AL strings
	alConfig.vendorString = qalGetString(AL_VENDOR);
	alConfig.rendererString = qalGetString(AL_RENDERER);
	alConfig.versionString = qalGetString(AL_VERSION);
	alConfig.extensionsString = qalGetString(AL_EXTENSIONS);

	// Get device name
	alConfig.deviceName = qalcGetString(alwState.hDevice, ALC_DEVICE_SPECIFIER);

	// Initialize extensions
	ALW_InitExtensions();

	return true;
}

/*
 =================
 ALW_Shutdown
 =================
*/
void ALW_Shutdown (void){

	Com_Printf("Shutting down OpenAL subsystem\n", LOG_CLIENT);

	if (alwState.hALC)
	{
		if (qalcMakeContextCurrent)
		{
			Com_Printf("...alcMakeContextCurrent( NULL ): ", LOG_CLIENT);
			if (!qalcMakeContextCurrent(NULL))
				Com_Printf("failed\n", LOG_CLIENT);
			else
				Com_Printf("succeeded\n", LOG_CLIENT);
		}

		if (qalcDestroyContext)
		{
			Com_Printf("...destroying AL context\n", LOG_CLIENT);
			qalcDestroyContext(alwState.hALC);
		}

		alwState.hALC = NULL;
	}

	if (alwState.hDevice)
	{
		if (qalcCloseDevice)
		{
			Com_Printf("...closing device\n", LOG_CLIENT);
			qalcCloseDevice(alwState.hDevice);
		}

		alwState.hDevice = NULL;
	}

	QAL_Shutdown();

	memset(&alConfig, 0, sizeof(alConfig_t));
	memset(&alwState, 0, sizeof(alwState_t));
}

#endif
