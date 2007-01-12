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
// snd_mem.c: sound caching

#include "client.h"
#include "snd_loc.h"

//int			cache_full_cycle;

byte *S_Alloc (int size);

/*
================
ResampleSfx
================
*/
void ResampleSfx (sfx_t *sfx, int inrate, int inwidth, byte *data)
{
	int		outcount;
	int		srcsample;
	float	stepscale;
	int		i;
	int		sample;
	uint32	samplefrac, fracstep;
	sfxcache_t	*sc;
	
	sc = sfx->cache;
	if (!sc)
		return;

	stepscale = (float)inrate / dma.speed;	// this is usually 0.5, 1, or 2

	outcount = (int)(sc->length / stepscale);
	
	if (outcount == 0)
	{
		Com_Printf ("ResampleSfx: Invalid sound file '%s' (zero length)\n", LOG_CLIENT|LOG_WARNING, sfx->name);
		//free at next opportunity
		//sfx->registration_sequence = 0;
		Z_Free (sfx->cache);
		sfx->cache = NULL;
		return;
	}

	sc->length = outcount;

	if (sc->loopstart != -1)
		sc->loopstart = (int)(sc->loopstart / stepscale);

	sc->speed = dma.speed;
	if (s_loadas8bit->intvalue)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

// resample / decimate to the current source rate

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		//Com_DPrintf ("ResampleSfx: special case used.\n");
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		//Com_Printf ("WARNING: ResampleSfx: general case used, step %f in %d sc %d\n", stepscale, inwidth, sc->width);
		samplefrac = 0;
		fracstep = (int)(stepscale*256);
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((int16 *)data)[srcsample] );
			else
				sample = (int32)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((int16 *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

//=============================================================================

#ifdef USE_OPENAL

static void S_OpenAL_UploadSound (byte *data, int width, int channels, sfx_t *sfx)
{
	int		size;

	// Calculate buffer size
	size = sfx->samples * width * channels;

	// Set buffer format
	if (width == 2)
	{
		if (channels == 2)
			sfx->format = AL_FORMAT_STEREO16;
		else
			sfx->format = AL_FORMAT_MONO16;
	}
	else
	{
		if (channels == 2)
			sfx->format = AL_FORMAT_STEREO8;
		else
			sfx->format = AL_FORMAT_MONO8;
	}

	// Upload the sound
	qalGenBuffers(1, &sfx->bufferNum);
	qalBufferData(sfx->bufferNum, sfx->format, data, size, sfx->rate);
}

static qboolean S_OpenAL_LoadWAV (const char *name, byte **wav, wavInfo_t *info);
qboolean S_OpenAL_LoadSound (sfx_t *sfx)
{
    char		name[MAX_QPATH];
	byte		*data;
	wavInfo_t	info;

	if (sfx->name[0] == '*')
		return false;

	// See if still in memory
	if (sfx->loaded)
		return true;

	// Load it from disk
	if (sfx->name[0] == '#')
		Com_sprintf(name, sizeof(name), "%s", &sfx->name[1]);
	else
		Com_sprintf(name, sizeof(name), "sound/%s", sfx->name);

	if (!S_OpenAL_LoadWAV(name, &data, &info))
	{
		Com_DPrintf ("WARNING: couldn't find sound '%s'\n", name);
		return false;
		//S_CreateDefaultSound(&data, &info);
	}

	// Load it in
	sfx->loaded = true;
	sfx->samples = info.samples;
	sfx->rate = info.rate;

	S_OpenAL_UploadSound(data, info.width, info.channels, sfx);

	Z_Free(data);

	return true;
}

#endif

/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[MAX_QPATH];
	byte	*data;
	wavinfo_t	info;
	int		len;
	float	stepscale;
	sfxcache_t	*sc;
	int		size;
	char	*name;

	if (s->name[0] == '*')
		return NULL;

// see if still in memory
	sc = s->cache;
	if (sc)
		return sc;

//Com_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
	if (s->truename)
		name = s->truename;
	else
		name = s->name;

	if (name[0] == '#')
		strcpy(namebuffer, &name[1]);
	else
		Com_sprintf (namebuffer, sizeof(namebuffer), "sound/%s", name);

//	Com_Printf ("loading %s\n",namebuffer);

	size = FS_LoadFile (namebuffer, (void **)&data);

	if (!data)
	{
		s->cache = NULL;
		Com_DPrintf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	//if (!openal_active)
	{

		info = GetWavinfo (s->name, data, size);
		if (info.channels != 1)
		{
			Com_DPrintf ("%s is an unsupported stereo sample\n", s->name);
			FS_FreeFile (data);
			return NULL;
		}

		stepscale = (float)info.rate / dma.speed;	
		len = (int)(info.samples / stepscale);

		if (info.samples == 0 || len == 0)
		{
			Com_DPrintf ("WARNING: Zero length sound encountered: %s\n", s->name);
			FS_FreeFile (data);
			return NULL;
		}

		len = len * info.width * info.channels;
		sc = s->cache = Z_TagMalloc (len + sizeof(sfxcache_t), TAGMALLOC_CLIENT_SOUNDCACHE);
	}
	//else
	//{
	//	sc = s->cache = Z_TagMalloc (sizeof(sfxcache_t), TAGMALLOC_CLIENT_SOUNDCACHE);
	//}
	
	if (!sc)
	{
		FS_FreeFile (data);
		return NULL;
	}

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx (s, sc->speed, sc->width, data + info.dataofs);

	FS_FreeFile (data);

	return sc;
}



/*
===============================================================================

WAV loading

===============================================================================
*/


byte	*data_p;
byte 	*iff_end;
byte 	*last_chunk;
byte 	*iff_data;
int 	iff_chunk_len;


int16 GetLittleShort(void)
{
	int16 val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

int32 GetLittleLong(void)
{
	int32 val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

void FindNextChunk(char *name)
{
	for (;;)
	{
		data_p = last_chunk;

		/*if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}*/
		
		data_p += 4;

		//r1: fix
		if (data_p >= iff_end)
		{
			data_p = NULL;
			return;
		}

		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %i length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((const char *)data_p, name, 4))
			return;
	}
}

void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}


void DumpChunks(void)
{
	char	str[5];
	
	str[4] = 0;
	data_p=iff_data;
	do
	{
		memcpy (str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Com_Printf ("%p : %s (%d)\n", LOG_CLIENT, data_p - 4, str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

#ifdef USE_OPENAL
static qboolean S_OpenAL_LoadWAV (const char *name, byte **wav, wavInfo_t *info)
{
	byte	*buffer, *out;
	int		length;

	length = FS_LoadFile(name, (void **)&buffer);
	if (!buffer)
		return false;

	iff_data = buffer;
	iff_end = buffer + length;

	// Find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !memcmp((void *)(data_p+8), "WAVE", 4)))
	{
		Com_DPrintf("S_LoadWAV: missing 'RIFF/WAVE' chunks (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	// Get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Com_DPrintf("S_LoadWAV: missing 'fmt ' chunk (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	data_p += 8;

	if (GetLittleShort() != 1)
	{
		Com_DPrintf("S_LoadWAV: Microsoft PCM format only (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	info->channels = GetLittleShort();
	if (info->channels != 1)
	{
		Com_DPrintf("S_LoadWAV: only mono WAV files supported (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	info->rate = GetLittleLong();

	data_p += 4+2;

	info->width = GetLittleShort() / 8;
	if (info->width != 1 && info->width != 2)
	{
		Com_DPrintf("S_LoadWAV: only 8 and 16 bit WAV files supported (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	// Find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Com_DPrintf("S_LoadWAV: missing 'data' chunk (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	data_p += 4;
	info->samples = GetLittleLong() / info->width;

	if (info->samples <= 0)
	{
		Com_DPrintf("S_LoadWAV: file with 0 samples (%s)\n", name);
		FS_FreeFile(buffer);
		return false;
	}

	// Load the data
	*wav = out = Z_TagMalloc(info->samples * info->width, TAGMALLOC_CLIENT_SOUNDCACHE);
	memcpy(out, buffer + (data_p - buffer), info->samples * info->width);

	FS_FreeFile(buffer);

	return true;
}
#endif

/*
============
GetWavinfo
============
*/
wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset (&info, 0, sizeof(info));

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!(data_p && !strncmp((const char *)data_p+8, "WAVE", 4)))
	{
		Com_Printf("GetWavinfo: Missing RIFF/WAVE chunks (%s)\n", LOG_CLIENT, name);
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk("fmt ");
	if (!data_p)
	{
		Com_Printf("GetWavinfo: Missing fmt chunk (%s)\n", LOG_CLIENT, name);
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Com_Printf("GetWavinfo: Microsoft PCM format only (%s)\n", LOG_CLIENT, name);
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopstart = GetLittleLong();
//		Com_Printf("loopstart=%d\n", sfx->loopstart);

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if ((data_p - wav) + 32 <= wavlength && !strncmp ((const char *)data_p + 28, "mark", 4))
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopstart + i;
//				Com_Printf("looped length: %i\n", i);
			}
		}
	}
	else
		info.loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Com_Printf("GetWavinfo: Missing data chunk (%s)\n", LOG_CLIENT, name);
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Com_Error (ERR_DROP, "Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = (int)(data_p - wav);
	
	return info;
}
