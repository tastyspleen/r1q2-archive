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
// snd_dma.c -- main control for any streaming sound output device

#include "client.h"
#include "../qcommon/redblack.h"
#include "snd_loc.h"

void S_Play(void);
void S_SoundList(void);
void S_Update_(void);
void S_StopAllSounds(void);

/* Factor to control attenuation of audio.
We'll divide all coordinates by this factor each time we update
the source positions. OpenAL does provide a cleaner way to do
this, but it changed recently. */
#define DISTANCE_FACTOR 50.0


// =======================================================================
// Internal sound data & structures
// =======================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
static int			s_registration_sequence;

channel_t   channels[MAX_CHANNELS];

static int			sound_started=0;

dma_t		dma;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;

static qboolean	s_registering;

static int	soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define		MAX_SFX		(MAX_SOUNDS*2)
static sfx_t		known_sfx[MAX_SFX];
static int			num_sfx;

static struct rbtree *knownsounds;

#define		MAX_PLAYSOUNDS	128
static playsound_t	s_playsounds[MAX_PLAYSOUNDS];
static playsound_t	s_freeplays;
playsound_t	s_pendingplays;

#ifdef USE_OPENAL
alindex_t alindex[MAX_OPENAL_SOURCES];
#endif

static int			s_beginofs;

cvar_t		*s_volume;
cvar_t		*s_testsound;
cvar_t		*s_loadas8bit;
cvar_t		*s_khz;
cvar_t		*s_primary;

static cvar_t		*s_show;
static cvar_t		*s_mixahead;
static cvar_t		*s_ambient;

cvar_t		*s_focusfree = &uninitialized_cvar;
//cvar_t		*s_dx8;


int		s_rawend;
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];


// ====================================================================
// User-setable variables
// ====================================================================


void S_SoundInfo_f(void)
{
	if (!sound_started)
	{
		Com_Printf ("sound system not started\n", LOG_CLIENT);
		return;
	}
	
    Com_Printf("%5d stereo\n", LOG_CLIENT, dma.channels - 1);
    Com_Printf("%5d samples\n", LOG_CLIENT, dma.samples);
    Com_Printf("%5d samplepos\n", LOG_CLIENT, dma.samplepos);
    Com_Printf("%5d samplebits\n", LOG_CLIENT, dma.samplebits);
    Com_Printf("%5d submission_chunk\n", LOG_CLIENT, dma.submission_chunk);
    Com_Printf("%5d speed\n", LOG_CLIENT, dma.speed);
    Com_Printf("0x%x dma buffer\n", LOG_CLIENT, dma.buffer);
}



/*
================
S_Init
================
*/
void S_Init (qboolean fullInit)
{
	cvar_t	*cv;

	Com_Printf("\n------- sound initialization -------\n", LOG_CLIENT|LOG_NOTICE);

	knownsounds = rbinit ((int (*)(const void *, const void *))strcmp, 0);

	cv = Cvar_Get ("s_initsound", "1", 0);
	if (!cv->intvalue)
		Com_Printf ("not initializing.\n", LOG_CLIENT|LOG_NOTICE);
	else
	{
		if ((int)cv->intvalue == 2)
		{
			if (!OpenAL_Init ())
			{
				sound_started = 1;
				S_StartLocalSound ("openalinit.wav");
			}
			else
			{
				Com_Printf ("OpenAL failed to initialize; no sound available\n", LOG_CLIENT);
			}
		} else {
			s_volume = Cvar_Get ("s_volume", "0.5", CVAR_ARCHIVE);
			s_khz = Cvar_Get ("s_khz", "22", CVAR_ARCHIVE);
			s_loadas8bit = Cvar_Get ("s_loadas8bit", "0", CVAR_ARCHIVE);
			s_mixahead = Cvar_Get ("s_mixahead", "0.2", CVAR_ARCHIVE);
			s_show = Cvar_Get ("s_show", "0", 0);
			s_ambient = Cvar_Get ("s_ambient", "1", 0);
			s_testsound = Cvar_Get ("s_testsound", "0", 0);
			s_primary = Cvar_Get ("s_primary", "0", CVAR_ARCHIVE);	// win32 specific

			s_focusfree = Cvar_Get ("s_focusfree", "0", 0);
			//s_dx8 = Cvar_Get ("s_dx8", "0", CVAR_ARCHIVE);

			Cmd_AddCommand("play", S_Play);
			Cmd_AddCommand("stopsound", S_StopAllSounds);
			Cmd_AddCommand("soundlist", S_SoundList);
			Cmd_AddCommand("soundinfo", S_SoundInfo_f);

			if (!SNDDMA_Init(fullInit))
				return;

			S_InitScaletable ();

			sound_started = 1;
			num_sfx = 0;

			soundtime = 0;
			paintedtime = 0;

			Com_Printf ("sound sampling rate: %i\n", LOG_CLIENT|LOG_NOTICE, dma.speed);

			S_StopAllSounds ();
		}
	}

	Com_Printf("------------------------------------\n", LOG_CLIENT|LOG_NOTICE);
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	int		i;
	sfx_t	*sfx;

	if (!sound_started)
		return;

	// free all sounds
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;
		if (sfx->cache)
			Z_Free (sfx->cache);
		rbdelete (sfx->name, knownsounds);
	}

	memset (known_sfx, 0, sizeof(known_sfx));

	rbdestroy (knownsounds);

	num_sfx = 0;
	sound_started = 0;

	if (openal_active)
	{
#ifdef USE_OPENAL
		OpenAL_Shutdown ();
#endif
	}
	else
	{
		SNDDMA_Shutdown();
		Cmd_RemoveCommand("play");
		Cmd_RemoveCommand("stopsound");
		Cmd_RemoveCommand("soundlist");
		Cmd_RemoveCommand("soundinfo");
	}
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
sfx_t *S_FindName (char *name, qboolean create)
{
	int		i;
	sfx_t	*sfx;
	void	**data;

	if (!name)
		Com_Error (ERR_FATAL, "S_FindName: NULL\n");

	// see if already loaded
	/*for (i=0 ; i < num_sfx ; i++)
		if (!strcmp(known_sfx[i].name, name))
		{
			return &known_sfx[i];
		}*/

	data = rbfind (name, knownsounds);
	if (data)
	{
		sfx = *(sfx_t **)data;
		return sfx;
	}

	if (!name[0])
		Com_Error (ERR_FATAL, "S_FindName: empty name\n");

	if (!create)
		return NULL;

	// find a free sfx
	for (i=0 ; i < num_sfx ; i++)
		if (!known_sfx[i].name[0])
//			registration_sequence < s_registration_sequence)
			break;

	if (i == num_sfx)
	{
		if (num_sfx == MAX_SFX)
			Com_Error (ERR_FATAL, "S_FindName: out of sfx_t");
		num_sfx++;
	}

	if (strlen(name) >= MAX_QPATH-1)
		Com_Error (ERR_FATAL, "Sound name too long: %s", name);

	sfx = &known_sfx[i];

	sfx->cache = NULL;
	sfx->truename = NULL;
	strcpy (sfx->name, name);
	sfx->registration_sequence = s_registration_sequence;

	data = rbsearch (sfx->name, knownsounds);
	*data = sfx;
	
	return sfx;
}


/*
==================
S_AliasName

==================
*/
sfx_t *S_AliasName (char *aliasname, char *truename)
{
	sfx_t	*sfx;
	void	**data;
	int		i;

	//s = Z_TagMalloc (MAX_QPATH, TAGMALLOC_CLIENT_SFX);
	//strcpy (s, truename);

	// find a free sfx
	for (i=0 ; i < num_sfx ; i++)
		if (!known_sfx[i].name[0])
			break;

	if (i == num_sfx)
	{
		if (num_sfx == MAX_SFX)
			Com_Error (ERR_FATAL, "S_FindName: out of sfx_t");
		num_sfx++;
	}
	
	sfx = &known_sfx[i];
	//memset (sfx, 0, sizeof(*sfx));
	sfx->cache = NULL;
	strcpy (sfx->name, aliasname);
	sfx->registration_sequence = s_registration_sequence;
	sfx->truename = CopyString (truename, TAGMALLOC_CLIENT_SFX);

	data = rbsearch (sfx->name, knownsounds);
	*data = sfx;

	return sfx;
}


/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration (void)
{
	s_registration_sequence++;
	s_registering = true;
}

/*
==================
S_RegisterSound

==================
*/
sfx_t *S_RegisterSound (char *name)
{
	sfx_t	*sfx;

	if (!sound_started)
		return NULL;

	sfx = S_FindName (name, true);
	sfx->registration_sequence = s_registration_sequence;

	if (!s_registering)
		S_LoadSound (sfx);

	return sfx;
}


/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration (void)
{
	int		i;
	sfx_t	*sfx;
	int		size;

	// free any sounds not from this registration sequence
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;

		if (sfx->registration_sequence != s_registration_sequence)
		{	// don't need this sound
			if (sfx->cache)				// it is possible to have a leftover
				Z_Free (sfx->cache);	// from a server that didn't finish loading
			rbdelete (sfx->name, knownsounds);
			sfx->name[0] = 0;
			//memset (sfx, 0, sizeof(*sfx));
		}
		else
		{	// make sure it is paged in
			if (sfx->cache)
			{
				size = sfx->cache->length*sfx->cache->width;
				Com_PageInMemory ((byte *)sfx->cache, size);
			}
		}
	}

	// load everything in
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;

		S_LoadSound (sfx);
	}

	s_registering = false;
}


//=============================================================================

/*
=================
S_PickChannel
=================
*/
channel_t *S_PickChannel(int entnum, int entchannel)
{
    int			ch_idx;
    int			first_to_die;
    int			life_left;
	channel_t	*ch;

	if (entchannel<0)
		Com_Error (ERR_DROP, "S_PickChannel: entchannel<0");

// Check for replacement sound, or find the best one to replace
    first_to_die = -1;
    life_left = 0x7fffffff;
    for (ch_idx=0 ; ch_idx < MAX_CHANNELS ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& channels[ch_idx].entchannel == entchannel)
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	memset (ch, 0, sizeof(*ch));

    return ch;
}       

/*
=================
S_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
void S_SpatializeOrigin (vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol)
{
    vec_t		dot;
    vec_t		dist;
    vec_t		lscale, rscale, scale;
    vec3_t		source_vec;

	if (cls.state != ca_active)
	{
		*left_vol = *right_vol = 255;
		return;
	}

// calculate stereo seperation and distance attenuation
	VectorSubtract(origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= SOUND_FULLVOLUME;
	if (FLOAT_LT_ZERO(dist))
		dist = 0;			// close enough to be at full volume

	dist *= dist_mult;		// different attenuation levels

	dot = DotProduct(listener_right, source_vec);

	if (dma.channels == 1 || !dist_mult)
	{ // no attenuation = no spatialization
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 0.5 * (1.0 + dot);
		lscale = 0.5*(1.0 - dot);
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	*right_vol = (int) (master_vol * scale);
	if (*right_vol < 0)
		*right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (int) (master_vol * scale);
	if (*left_vol < 0)
		*left_vol = 0;
}

/*
=================
S_Spatialize
=================
*/
void S_Spatialize(channel_t *ch)
{
	vec3_t		origin;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum+1)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	if (ch->fixed_origin)
	{
		VectorCopy (ch->origin, origin);
	}
	else
		CL_GetEntitySoundOrigin (ch->entnum, origin);

	S_SpatializeOrigin (origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}           


/*
=================
S_AllocPlaysound
=================
*/
playsound_t *S_AllocPlaysound (void)
{
	playsound_t	*ps;

	ps = s_freeplays.next;
	if (ps == &s_freeplays)
		return NULL;		// no free playsounds

	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;
	
	return ps;
}


/*
=================
S_FreePlaysound
=================
*/
void S_FreePlaysound (playsound_t *ps)
{
	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}



/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void S_IssuePlaysound (playsound_t *ps)
{
	channel_t	*ch;
	sfxcache_t	*sc;

	if (s_show->intvalue)
		Com_Printf ("Issue %i\n", LOG_CLIENT, ps->begin);
	// pick a channel to play on
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if (!ch)
	{
		S_FreePlaysound (ps);
		return;
	}

	// spatialize
	if (ps->attenuation == ATTN_STATIC)
		ch->dist_mult = ps->attenuation * 0.001;
	else
		ch->dist_mult = ps->attenuation * 0.0005;
	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy (ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	S_Spatialize(ch);

	ch->pos = 0;
	sc = S_LoadSound (ch->sfx);
    ch->end = paintedtime + sc->length;

	// free the playsound
	S_FreePlaysound (ps);
}

struct sfx_s *S_RegisterSexedSound (char *base, int entnum)
{
	int				n;
	int				len;
	char			*p;
	struct sfx_s	*sfx;
//	FILE			*f;
	char			model[MAX_QPATH];
	char			sexedFilename[MAX_QPATH];
	char			maleFilename[MAX_QPATH];

	// determine what model the client is using
	model[0] = 0;
	//n = CS_PLAYERSKINS + ent->number - 1;
	n = CS_PLAYERSKINS + entnum - 1;
	if (cl.configstrings[n][0])
	{
		p = strchr(cl.configstrings[n], '\\');
		if (p)
		{
			p += 1;
			Q_strncpy(model, p, sizeof(model)-1);
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}

	// if we can't figure it out, they're male
	if (!model[0])
		strcpy(model, "male");

	// see if we already know of the model specific sound
	Com_sprintf (sexedFilename, sizeof(sexedFilename), "#players/%s/%s", model, base+1);
	sfx = S_FindName (sexedFilename, false);

	if (!sfx)
	{
		// no, so see if it exists
		//FS_FOpenFile (&sexedFilename[1], &f, false);
		len = FS_LoadFile (&sexedFilename[1], NULL);
		if (len != -1)
		{
			// yes, close the file and register it
			//FS_FCloseFile (f);
			sfx = S_RegisterSound (sexedFilename);
		}
		else
		{
			// no, revert to the male sound in the pak0.pak
			Com_sprintf (maleFilename, sizeof(maleFilename), "player/male/%s", base+1);
			sfx = S_AliasName (sexedFilename, maleFilename);
		}
	}

	return sfx;
}


// =======================================================================
// Start a sound effect
// =======================================================================


/*
====================
S_StartSound

Validates the parms and ques the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs)
{
	sfxcache_t	*sc;
	int			vol;
	playsound_t	*ps, *sort;
	int			start;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (sfx->name[0] == '*')
		sfx = S_RegisterSexedSound(sfx->name, entnum);

	//Com_DPrintf ("S_StartSound: %s\n", sfx->name);

	// make sure the sound is loaded
	sc = S_LoadSound (sfx);
	if (!sc)
		return;		// couldn't load the sound's data

#ifdef USE_OPENAL
	if (openal_active)
	{
		int i;
		ALint sourceNum;

		i = OpenAL_GetFreeAlIndex();
		if (i == -1)
		{
			Com_Printf ("OpenAL: Warning: Out of alindexes!\n", LOG_CLIENT);
			return;
		}

		memset (&alindex[i], 0, sizeof(alindex_t));

		sourceNum = OpenAL_GetFreeSource();
		if (sourceNum == -1)
		{
			Com_DPrintf ("OpenAL: Warning: Out of sources!\n", LOG_CLIENT);
		}
		else
		{
			vec3_t torigin;

			alindex[i].inuse = true;

			if (origin)
			{
				alindex[i].fixed_origin = true;
				VectorCopy (origin, alindex[i].origin);
				VectorScale (alindex[i].origin, OPENAL_SCALE_VALUE, alindex[i].origin);
			}
			else
			{
				alindex[i].fixed_origin = false;
			}

			alSourcei (g_Sources[sourceNum], AL_LOOPING, AL_FALSE);
			OpenAL_CheckForError();

			//Com_Printf ("OpenAL: Playing buffer %d through source %d\n", sfx->cache->bufferNum, sourceNum);
			if (!attenuation)
			{
				Com_DPrintf ("no attn, ent = %d, fvol = %f\n", entnum, fvol);

				alSourcef (g_Sources[sourceNum], AL_GAIN, 1.0f);
				OpenAL_CheckForError();

				alSourcef (g_Sources[sourceNum], AL_ROLLOFF_FACTOR, 0.0f);
				OpenAL_CheckForError();

				alSourcei (g_Sources[sourceNum], AL_SOURCE_RELATIVE, AL_TRUE);
				OpenAL_CheckForError();

				/*alSourcef (g_Sources[sourceNum], AL_REFERENCE_DISTANCE, 128.0f);
				OpenAL_CheckForError();*/

				alSourcefv (g_Sources[sourceNum], AL_POSITION, vec3_origin);
				OpenAL_CheckForError();

				alindex[i].fixed_origin = true;
				VectorCopy (vec3_origin, alindex[i].origin);

				entnum = 0;//cl.playernum+1;
			} else {
				float gain;

				if (!origin)
				{	
					CL_GetEntitySoundOrigin (entnum, torigin);
					origin = torigin;
				}
				else
				{
					VectorCopy (origin, alindex[i].origin);
					VectorScale (alindex[i].origin, OPENAL_SCALE_VALUE, alindex[i].origin);
					alindex[i].fixed_origin = true;
				}

				VectorScale (origin, OPENAL_SCALE_VALUE, origin);

				alSourcefv (g_Sources[sourceNum], AL_POSITION, origin);
				OpenAL_CheckForError();

				/*alSourcef (g_Sources[sourceNum], AL_ROLLOFF_FACTOR, 1.0f);
				OpenAL_CheckForError();*/

				alSourcef (g_Sources[sourceNum], AL_ROLLOFF_FACTOR, 1.0f * attenuation);
				OpenAL_CheckForError();

				/*alSourcef (g_Sources[sourceNum], AL_REFERENCE_DISTANCE, 256.0f * fvol);
				OpenAL_CheckForError();*/

				gain = fvol / 1.0;
				//gain *= 0.2;

				alSourcef (g_Sources[sourceNum], AL_GAIN, gain);
				OpenAL_CheckForError();

				alSourcei (g_Sources[sourceNum], AL_SOURCE_RELATIVE, AL_FALSE);
				OpenAL_CheckForError();
			}

			alindex[i].sourceIndex = sourceNum;
			alindex[i].entnum = entnum;
			alindex[i].attenuation = attenuation;
			alindex[i].loopsound = false;
			alindex[i].lastloopframe = 0;

			if (AL_Attenuated (i))
			{
				alindex[i].inuse = false;
				return;
			}

			alSourcei (g_Sources[sourceNum], AL_BUFFER, g_Buffers[sfx->cache->bufferNum].buffer);
			OpenAL_CheckForError();

			alSourcePlay (g_Sources[sourceNum]);
			OpenAL_CheckForError();
		}

		return;
	}
#endif

	vol = fvol*255;

	// make the playsound_t
	ps = S_AllocPlaysound ();
	if (!ps)
		return;

	if (origin)
	{
		VectorCopy (origin, ps->origin);
		ps->fixed_origin = true;
	}
	else
		ps->fixed_origin = false;

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->volume = vol;
	ps->sfx = sfx;

	// drift s_beginofs
	start = cl.frame.servertime * 0.001 * dma.speed + s_beginofs;
	if (start < paintedtime)
	{
		start = paintedtime;
		s_beginofs = start - (cl.frame.servertime * 0.001 * dma.speed);
	}
	else if (start > paintedtime + 0.3 * dma.speed)
	{
		start = paintedtime + 0.1 * dma.speed;
		s_beginofs = start - (cl.frame.servertime * 0.001 * dma.speed);
	}
	else
	{
		s_beginofs-=10;
	}

	if (!timeofs)
		ps->begin = paintedtime;
	else
		ps->begin = start + timeofs * dma.speed;

	// sort into the pending sound list
	for (sort = s_pendingplays.next ; 
		sort != &s_pendingplays && sort->begin < ps->begin ;
		sort = sort->next)
			;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound (char *sound)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;
		
	sfx = S_RegisterSound (sound);
	if (!sfx)
	{
		Com_Printf ("S_StartLocalSound: can't cache %s\n", LOG_CLIENT, sound);
		return;
	}

	//r1: don't use attenuation calculations on local sounds
	S_StartSound (NULL, cl.playernum + 1, 0, sfx, 1, 0, 0);
}


/*
==================
S_ClearBuffer
==================
*/
void S_ClearBuffer (void)
{
	int		clear;
		
	if (!sound_started)
		return;

#ifdef USE_OPENAL
	if (openal_active)
	{
		OpenAL_DestroyBuffers();
		return;
	}
#endif

	s_rawend = 0;

	if (dma.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	SNDDMA_BeginPainting ();
	if (dma.buffer)
		memset(dma.buffer, clear, dma.samples * dma.samplebits/8);
	SNDDMA_Submit ();
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds(void)
{
	int		i;

	if (!sound_started)
		return;

	// clear all the playsounds
	memset(s_playsounds, 0, sizeof(s_playsounds));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for (i=0 ; i<MAX_PLAYSOUNDS ; i++)
	{
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	// clear all the channels
	memset(channels, 0, sizeof(channels));

	S_ClearBuffer ();
}

/*
==================
S_AddLoopSounds

Entities with a ->sound field will generated looped sounds
that are automatically started, stopped, and merged together
as the entities are sent to the client
==================
*/
void S_AddLoopSounds (void)
{
	int			i, j;
	int			sounds[MAX_EDICTS];
	int			left, right, left_total, right_total;
	channel_t	*ch;
	sfx_t		*sfx;
	sfxcache_t	*sc;
	int			num;
	entity_state_t	*ent;
	vec3_t		origin;

	if (cl_paused->intvalue)
		return;

	if (cls.state != ca_active)
		return;

	if (!cl.sound_prepped)
		return;

	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];
		sounds[i] = ent->sound;
	}

	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		if (!sounds[i])
			continue;

		sfx = cl.sound_precache[sounds[i]];
		if (!sfx)
			continue;		// bad sound effect
		sc = sfx->cache;
		if (!sc)
			continue;

		num = (cl.frame.parse_entities + i)&(MAX_PARSE_ENTITIES-1);
		ent = &cl_parse_entities[num];

		//cmodel sound fix
		if (ent->solid == 31)
		{
			cmodel_t	*model;

			model = cl.model_clip[ent->modelindex];

			//not loaded (deferred?)
			if (!model)
				continue;

			origin[0] = ent->origin[0]+0.5*(model->mins[0]+model->maxs[0]);
			origin[1] = ent->origin[1]+0.5*(model->mins[1]+model->maxs[1]);
			origin[2] = ent->origin[2]+0.5*(model->mins[2]+model->maxs[2]);
		}
		else
		{
			VectorCopy (ent->origin, origin);
		}

		// find the total contribution of all sounds of this type
		S_SpatializeOrigin (origin, 255.0, SOUND_LOOPATTENUATE,
			&left_total, &right_total);
		if (!openal_active)
		{
			for (j=i+1 ; j<cl.frame.num_entities ; j++)
			{
				if (sounds[j] != sounds[i])
					continue;
				sounds[j] = 0;	// don't check this again later

				num = (cl.frame.parse_entities + j)&(MAX_PARSE_ENTITIES-1);
				ent = &cl_parse_entities[num];

				S_SpatializeOrigin (ent->origin, 255.0, SOUND_LOOPATTENUATE, 
					&left, &right);
				left_total += left;
				right_total += right;
			}
		}

		if (left_total == 0 && right_total == 0)
			continue;		// not audible

#ifdef USE_OPENAL
		if (openal_active)
		{
			int		k;
			int		sourceNum;

			vec3_t	origin;

			for (k = 0; k < MAX_OPENAL_SOURCES; k++)
			{
				if (alindex[k].inuse && alindex[k].entnum == ent->number)
				{
					alindex[k].lastloopframe = cl.frame.serverframe;
					goto skipsound;
				}
			}

			k = OpenAL_GetFreeAlIndex();
			if (k == -1)
			{
				Com_Printf ("OpenAL: Loopsound: Warning: Out of alindexes!\n", LOG_CLIENT);
				return;
			}

			memset (&alindex[k], 0, sizeof(alindex_t));

			sourceNum = OpenAL_GetFreeSource();

			if (sourceNum == -1)
			{
				Com_DPrintf ("OpenAL: Loopsound: Warning: Out of sources!\n", LOG_CLIENT);
				continue;
			}

			VectorCopy (ent->origin, origin);
			VectorScale (origin, OPENAL_SCALE_VALUE, origin);

			alSourcefv (g_Sources[sourceNum], AL_POSITION, origin);
			OpenAL_CheckForError();

			alSourcef (g_Sources[sourceNum], AL_ROLLOFF_FACTOR, 1.0f);
			OpenAL_CheckForError();

			alSourcef (g_Sources[sourceNum], AL_GAIN, 1.0);
			OpenAL_CheckForError();

			alSourcei (g_Sources[sourceNum], AL_SOURCE_RELATIVE, AL_FALSE);
			OpenAL_CheckForError();

			alSourcef (g_Sources[sourceNum], AL_REFERENCE_DISTANCE, 0.5f);
			OpenAL_CheckForError();

			alSourcei (g_Sources[sourceNum], AL_LOOPING, AL_TRUE);
			OpenAL_CheckForError();

			alSourcei (g_Sources[sourceNum], AL_BUFFER, g_Buffers[sfx->cache->bufferNum].buffer);
			OpenAL_CheckForError();

			alindex[k].inuse = true;
			alindex[k].loopsound = true;
			alindex[k].fixed_origin = false;
			alindex[k].sourceIndex = sourceNum;
			alindex[k].entnum = ent->number;
			alindex[k].attenuation = ATTN_STATIC;
			alindex[k].lastloopframe = cl.frame.serverframe;

			alSourcePlay (g_Sources[sourceNum]);
			OpenAL_CheckForError();
		}
		else
		{
#endif
			// allocate a channel
			ch = S_PickChannel(0, 0);
			if (!ch)
				return;

			if (left_total > 255)
				left_total = 255;
			if (right_total > 255)
				right_total = 255;
			ch->leftvol = left_total;
			ch->rightvol = right_total;
			ch->autosound = true;	// remove next frame
			ch->sfx = sfx;
			ch->pos = paintedtime % sc->length;
			ch->end = paintedtime + sc->length - ch->pos;
#ifdef USE_OPENAL
		}
skipsound:;
#endif
	}
}

//=============================================================================

/*
============
S_RawSamples

Cinematic streaming and voice over network
============
*/
void S_RawSamples (int samples, int rate, int width, int channels, byte *data)
{
	int		i;
	int		src, dst;
	float	scale;

	if (!sound_started || openal_active)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;
	scale = (float)rate / dma.speed;

//Com_Printf ("%i < %i < %i\n", soundtime, paintedtime, s_rawend);
	if (channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{	// optimized case
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    LittleShort(((short *)data)[i*2]) << 8;
				s_rawsamples[dst].right =
				    LittleShort(((short *)data)[i*2+1]) << 8;
			}
		}
		else
		{
			for (i=0 ; ; i++)
			{
				src = i*scale;
				if (src >= samples)
					break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    LittleShort(((short *)data)[src*2]) << 8;
				s_rawsamples[dst].right =
				    LittleShort(((short *)data)[src*2+1]) << 8;
			}
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    LittleShort(((short *)data)[src]) << 8;
			s_rawsamples[dst].right =
			    LittleShort(((short *)data)[src]) << 8;
		}
	}
	else if (channels == 2 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    ((char *)data)[src*2] << 16;
			s_rawsamples[dst].right =
			    ((char *)data)[src*2+1] << 16;
		}
	}
	else if (channels == 1 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    (((byte *)data)[src]-128) << 16;
			s_rawsamples[dst].right = (((byte *)data)[src]-128) << 16;
		}
	}
}

//=============================================================================

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int			i;
	int			total;
	channel_t	*ch;
//	channel_t	*combine;

	if (!sound_started)
		return;

	// if the laoding plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	if (cls.disable_screen)
	{
		S_ClearBuffer ();
		return;
	}

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

#ifdef USE_OPENAL
	if (openal_active)
	{
		ALfloat listener[3];
		ALfloat orientation[6];
		int i;

		VectorScale (listener_origin, OPENAL_SCALE_VALUE, listener);

		alListenerfv (AL_POSITION, listener);

		if (s_ambient->intvalue)
			S_AddLoopSounds ();

		orientation[0] = listener_forward[0];// * 0.02;
		orientation[1] = listener_forward[1];// * 0.02;
		orientation[2] = listener_forward[2];// * 0.02;
		orientation[3] = listener_up[0];// * 0.02;
		orientation[4] = listener_up[1];// * 0.02;
		orientation[5] = listener_up[2];// * 0.02;
		alListenerfv(AL_ORIENTATION, orientation);
		OpenAL_CheckForError();

		for (i = 0; i < MAX_OPENAL_SOURCES; i++)
		{
			if (alindex[i].inuse)
			{
				ALenum	state;

				if (alindex[i].loopsound)
				{
					if (alindex[i].lastloopframe < cl.frame.serverframe)
					{
						Com_Printf ("Loopsound %d not in this frame, stopping.\n", LOG_CLIENT, i);
						alindex[i].inuse = false;
						alSourceStop (g_Sources[alindex[i].sourceIndex]);
						OpenAL_CheckForError();
						continue;
					}
				}

				alGetSourcei (g_Sources[i], AL_SOURCE_STATE, &state);
				OpenAL_CheckForError();

				if (state == AL_STOPPED || state == AL_INITIAL)
				{
					alindex[i].inuse = false;
					continue;
				}

				if (AL_Attenuated (i))
				{
					Com_DPrintf ("Dropped %ssound %d, attenuated.\n", alindex[i].loopsound ? "loop" : "", i);
					alindex[i].inuse = false;
					alSourceStop (g_Sources[alindex[i].sourceIndex]);
					OpenAL_CheckForError();
					continue;
				}

				alSourcefv (g_Sources[alindex[i].sourceIndex], AL_POSITION, alindex[i].origin);
				OpenAL_CheckForError();
			}
		}

		return;
	}
#endif

	// rebuild scale tables if volume is modified
	if (s_volume->modified)
		S_InitScaletable ();

	//combine = NULL;

	// update spatialization for dynamic sounds	
	ch = channels;
	for (i=0 ; i<MAX_CHANNELS; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		if (ch->autosound)
		{	// autosounds are regenerated fresh each frame
			memset (ch, 0, sizeof(*ch));
			continue;
		}
		S_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
		{
			memset (ch, 0, sizeof(*ch));
			continue;
		}
	}

	// add loopsounds
	if (s_ambient->intvalue)
		S_AddLoopSounds ();

	//
	// debugging output
	//
	if (s_show->intvalue)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<MAX_CHANNELS; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol) )
			{
				Com_Printf ("%3i %3i %s\n", LOG_CLIENT, ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		
		Com_Printf ("----(%i)---- painted: %i\n", LOG_CLIENT, total, paintedtime);
	}

// mix some sound
	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;
	
	fullsamples = dma.samples / dma.channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/dma.channels;
}


void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started || openal_active)
		return;

	SNDDMA_BeginPainting ();

	if (!dma.buffer)
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Com_DPrintf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

// mix ahead of current position
	endtime = soundtime + s_mixahead->value * dma.speed;
//endtime = (soundtime + 4096) & ~4095;

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);
	samps = dma.samples >> (dma.channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

void S_Play(void)
{
	int 	i;
	char name[256];
	sfx_t	*sfx;
	
	i = 1;
	while (i<Cmd_Argc())
	{
		if (!strrchr(Cmd_Argv(i), '.'))
		{
			strncpy(name, Cmd_Argv(i), sizeof(name)-5);
			strcat(name, ".wav");
		}
		else
			strncpy(name, Cmd_Argv(i), sizeof(name)-1);

		if (strstr(name, "..") || name[0] == '/' || name[0] == '\\') {
			Com_Printf ("Bad filename %s\n", LOG_CLIENT, name);
			return;
		}

		sfx = S_RegisterSound(name);
		if (sfx) {
			S_StartSound(NULL, cl.playernum+1, 0, sfx, 1.0, 1.0, 0);
			//sfx->name[0] = '\0';
		}
		i++;
	}
}

void S_SoundList(void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;

	total = 0;
	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (!sfx->registration_sequence)
			continue;
		sc = sfx->cache;
		if (sc)
		{
			size = sc->length*sc->width*(sc->stereo+1);
			total += size;
			if (sc->loopstart >= 0)
				Com_Printf ("L", LOG_CLIENT);
			else
				Com_Printf (" ", LOG_CLIENT);
			Com_Printf("(%2db) %6i : %s\n", LOG_CLIENT,sc->width*8,  size, sfx->name);
		}
		else
		{
			if (sfx->name[0] == '*')
				Com_Printf("  placeholder : %s\n", LOG_CLIENT, sfx->name);
			else
				Com_Printf("  not loaded  : %s\n", LOG_CLIENT, sfx->name);
		}
	}
	Com_Printf ("Total resident: %i\n", LOG_CLIENT, total);
}

