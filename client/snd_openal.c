/*
	OpenAL Interface for r1q2.
*/

int openal_active = 0;

#include <float.h>
#include "../client/client.h"
#include "../client/snd_loc.h"
#include "../win32/winquake.h"
#include <memory.h>
#include <math.h>

#ifdef USE_OPENAL
//did openal init ok?
unsigned int openAlMaxSources = 0;
unsigned int openAlMaxBuffers = 0;


/*
Sources store locations, directions, and other attributes of
an object in 3D space and have a buffer associated with them
for playback. There are normally far more sources defined than
buffers. When the program wants to play a sound, it controls
execution through a source object. Sources are processed
independently from each other.
*/

/*
Buffers store compressed or un-compressed audio data. It is
common to initialize a large set of buffers when the program
first starts (or at non-critical times during execution --
between levels in a game, for instance). Buffers are referred to
by Sources. Data (audio sample data) is associated with buffers.
*/

/*
There is only one listener (per audio context). The listener
attributes are similar to source attributes, but are used to
represent where the user is hearing the audio from. The influence
of all the sources from the perspective of the listener is mixed
and played for the user.
*/

		// Array of Buffer IDs
ALuint			g_Sources[MAX_OPENAL_SOURCES];
int				g_SourceState[MAX_OPENAL_SOURCES];
OpenALBuffer_t	g_Buffers[MAX_OPENAL_BUFFERS];

char *GetALErrorString(ALenum err)
{
    switch(err)
    {
        case AL_NO_ERROR:
            return ("AL_NO_ERROR");
        break;

        case AL_INVALID_NAME:
            return ("AL_INVALID_NAME");
        break;

        case AL_INVALID_ENUM:
            return ("AL_INVALID_ENUM");
        break;

        case AL_INVALID_VALUE:
            return ("AL_INVALID_VALUE");
        break;

        case AL_INVALID_OPERATION:
            return ("AL_INVALID_OPERATION");
        break;

        case AL_OUT_OF_MEMORY:
            return ("AL_OUT_OF_MEMORY");
        break;

		default:
			return ("UNKNOWN");
		break;
    };
}

void OpenAL_CheckForError (void)
{
	int error;
	if ((error = alGetError()) != AL_NO_ERROR)
		Com_Printf ("OpenAL: ERROR: %s\n", GetALErrorString(error));
}

/*
==============
DisplayALError
==============
Show and clear OpenAL Library error message, if any.
*/
void DisplayALError (char *msg, int code)
{
	Com_Printf ("OpenAL: Error: %s(%.2x)\n", msg, code);
}

void OpenAL_DestroyBuffers (void)
{
	int i;
	for (i = 0; i < openAlMaxSources; i++) {
		alSourceStop (g_Sources[i]);
	}	
}

int OpenAL_GetFreeAlIndex (void)
{
	int i;
	for (i = 0; i < MAX_SOUNDS; i++)
	{
		if (!alindex[i].inuse)
			return i;
	}

	return -1;
}

void OpenAL_FreeAlIndexes (int index)
{
	int i;

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		if (alindex[i].sourceIndex == index)
			alindex[i].inuse = false;
	}	
}

ALint OpenAL_GetFreeSource (void)
{
	ALenum state;
	int i;

	for (i = 0; i < openAlMaxSources; i++)
	{
		alGetSourcei (g_Sources[i], AL_SOURCE_STATE, &state);
		OpenAL_CheckForError();
		if (state == AL_STOPPED || state == AL_INITIAL)
		{
			//Com_Printf ("OpenAL: Source %d has finished playing.\n", i);
			OpenAL_FreeAlIndexes (i);
			return i;
		}
	}

	return -1;
}

/*
===============
OpenAL_Shutdown
===============
Shut down the OpenAL interface, if it is running.
*/
void OpenAL_Shutdown (void)
{
	int i;
	int error;
	ALCcontext *Context;
	ALCdevice *Device;

	if (!openal_active)
		return;

	for (i = 0; i < openAlMaxSources; i++) {
		alSourceStop (g_Sources[i]);
		OpenAL_CheckForError();

		alSourcei (g_Sources[i], AL_BUFFER, 0);
		OpenAL_CheckForError();
	}

	alDeleteSources(openAlMaxSources, g_Sources);
	OpenAL_CheckForError();

	for (i = 0; i < openAlMaxBuffers; i++)
	{
		if (g_Buffers[i].inuse)
		{
			alDeleteBuffers(1, &g_Buffers[i].buffer);

			if ((error = alGetError()) != AL_NO_ERROR)
			{
				DisplayALError("alDeleteBuffers : ", error);
				return;
			}
		}
	}

	openAlMaxSources = 0;
	openAlMaxBuffers = 0;

	memset (alindex, 0, sizeof(alindex));

	//Get active context
	Context=alcGetCurrentContext();

	//Get device for active context
	Device=alcGetContextsDevice(Context);

	//Disable context
	alcMakeContextCurrent(NULL);

	//Release context(s)
	alcDestroyContext(Context);

	//Close device
	alcCloseDevice(Device);

	openal_active = 0;
	Com_Printf ("OpenAL: Shutdown complete.\n");
}

ALint OpenAL_GetFreeBuffer (void)
{
	int i;
	for (i = 0; i < openAlMaxBuffers; i++)
	{
		if (!g_Buffers[i].inuse)
		{
			g_Buffers[i].inuse = true;
			return i;
		}
	}

	return -1;
}
#endif

/*
============
OpenAL_Init
============
Try to initialize the OpenAL library. Return 0 on success or non-zero on failure.
Error message will be automatically printed if initialization fails.
*/
qboolean OpenAL_Init (void)
{
#ifndef USE_OPENAL
	return false;
#else
	int i;
	int	error;
	ALCcontext *Context;
	ALCdevice *Device;

	if (openal_active) {
		Com_Printf ("Error: OpenAL_Init when openal_active = 1\n");
		return 1;
	}

	Device = alcOpenDevice((ALubyte*)"DirectSound3D");

	if (Device == NULL)
	{
		Com_Printf ("OpenAL: DirectSound3D unavailable; cannot continue\n");
		return 1;
	}

	//Create context(s)
	Context=alcCreateContext(Device,NULL);

	if (Context == NULL)
	{
		Com_Printf ("OpenAL: unable to create rendering context; cannot continue\n");
		return 1;
	}

	//Set active context
	alcMakeContextCurrent(Context);

	if (alcGetError(Device) != ALC_NO_ERROR)
	{
		Com_Printf ("OpenAL: unable to activate rendering context; cannot continue\n");
		return 1;
	}

	// Clear Error Codes
	alGetError();
	alcGetError(Device);

	// Set Listener attributes

	// Position ...
	alListenerfv(AL_POSITION,listener_origin);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DisplayALError("alListenerfv POSITION : ", error);
		return 1;
	}

	// Orientation ...
	alListenerfv(AL_ORIENTATION,vec3_origin);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DisplayALError("alListenerfv ORIENTATION : ", error);
		return 1;
	}

	//alListenerf (AL_GAIN, 0);
	//OpenAL_CheckForError();

	{
		int major, minor;
		alcGetIntegerv (Device, ALC_MAJOR_VERSION, sizeof(major), &major);
		alcGetIntegerv (Device, ALC_MINOR_VERSION, sizeof(minor), &minor);
		Com_Printf ("...OpenAL %d.%d on %s\n", major, minor, alcGetString (Device, ALC_DEFAULT_DEVICE_SPECIFIER));
	}

	// Generate Buffers
	for (i = 0; i < MAX_OPENAL_BUFFERS ; i++)
	{
		alGenBuffers(1, &g_Buffers[i].buffer);
		if ((error = alGetError()) != AL_NO_ERROR)
		{
			DisplayALError("alGenBuffers :", error);
			return 1;
		}
	}

	openAlMaxBuffers = i;

	Com_Printf ("...Generated %d buffers.\n", openAlMaxBuffers);

	// Generate as many sources as possible (up to 64)
	for (i = 0; i < MAX_OPENAL_SOURCES ; i++)
	{
		alGenSources(1, &g_Sources[i]);
		if ((error = alGetError()) != AL_NO_ERROR)
			break;
	}

	openAlMaxSources = i;

	Com_Printf ("...Generated %d sources.\n", openAlMaxSources);

	if (openAlMaxSources <= 32)
		Com_Printf ("WARNING: Only %d sources available. This probably isn't enough, it is recommended you go back to DirectSound.\n", openAlMaxSources);

	// Load in samples to be used by Test functions

	// Load footsteps.wav
	/*alutLoadWAVFile("footsteps.wav",&format,&data,&size,&freq,&loop);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DisplayALError("alutLoadWAVFile footsteps.wav : ", error);
		// Delete Buffers
		alDeleteBuffers(MAX_CHANNELS, g_Buffers);
		return 1;
	}

	// Copy footsteps.wav data into AL Buffer 0
	alBufferData(g_Buffers[0],format,data,size,freq);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DisplayALError("alBufferData buffer 0 : ", error);
		// Delete buffers
		alDeleteBuffers(MAX_CHANNELS, g_Buffers);
		return 1;
	}

	// Unload footsteps.wav
	alutUnloadWAV(format,data,size,freq);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DisplayALError("alutUnloadWAV : ", error);
		// Delete buffers
		alDeleteBuffers(MAX_CHANNELS, g_Buffers);
		return 1;
	}*/

	openal_active = 1;

	return 0;
#endif
}
