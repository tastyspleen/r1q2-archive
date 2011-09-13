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

//FIXME OpenAL
#ifdef _WIN32
extern qboolean ActiveApp;
#endif

void S_Play(void);
void S_SoundList_f (void);
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

static int			s_beginofs;

cvar_t		*s_volume;
cvar_t		*s_testsound;
cvar_t		*s_loadas8bit;
cvar_t		*s_khz;
cvar_t		*s_primary;

cvar_t		*s_openal_extensions;
cvar_t		*s_openal_eax;

static cvar_t		*s_show;
static cvar_t		*s_mixahead;
static cvar_t		*s_ambient;

cvar_t		*s_focusfree = &uninitialized_cvar;
//cvar_t		*s_dx8;


int		s_rawend;
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

#ifdef USE_OPENAL
cvar_t		*s_openal_volume;
cvar_t		*s_openal_device;
static openal_listener_t	s_openal_listener;
static openal_channel_t		s_openal_channels[MAX_CHANNELS];
static int					s_openal_numChannels;
static int					s_openal_frameCount;

#ifdef _WIN32
const GUID			DSPROPSETID_EAX20_ListenerProperties = {0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};
const GUID			DSPROPSETID_EAX20_BufferProperties = {0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};
#endif

#endif


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
    Com_Printf("%p dma buffer\n", LOG_CLIENT, dma.buffer);
}

#ifdef USE_OPENAL
static void S_OpenAL_AllocChannels (void)
{
	openal_channel_t	*ch;
	int					i;

	for (i = 0, ch = s_openal_channels; i < MAX_CHANNELS; i++, ch++)
	{
		qalGenSources(1, &ch->sourceNum);

		if (qalGetError() != AL_NO_ERROR)
			break;

		s_openal_numChannels++;
	}
}
#endif

/*
================
S_Init
================
*/
void S_Init (int fullInit)
{
	cvar_t	*cv;

	if (!cl_quietstartup->intvalue || developer->intvalue)
		Com_Printf("\n------- sound initialization -------\n", LOG_CLIENT|LOG_NOTICE);

	knownsounds = rbinit ((int (EXPORT *)(const void *, const void *))strcmp, 0);

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

#ifdef USE_OPENAL
	s_openal_device = Cvar_Get ("s_openal_device", "", 0);
	s_openal_extensions = Cvar_Get ("s_openal_extensions", "1", 0);
	s_openal_eax = Cvar_Get ("s_openal_eax", "0", 0);
	s_openal_volume = Cvar_Get ("s_openal_volume", "1", 0);
#endif

	s_volume = Cvar_Get ("s_volume", "0.5", CVAR_ARCHIVE);
	s_show = Cvar_Get ("s_show", "0", 0);
	s_ambient = Cvar_Get ("s_ambient", "1", 0);

	cv = Cvar_Get ("s_initsound", "1", 0);
	if (!cv->intvalue)
		Com_Printf ("not initializing.\n", LOG_CLIENT|LOG_NOTICE);
	else
	{
		if (cv->intvalue == 2)
		{
#ifdef USE_OPENAL
			if (ALimp_Init ())
			{
				sound_started = 1;

				Cmd_AddCommand("play", S_Play);
				Cmd_AddCommand("stopsound", S_StopAllSounds);

				S_OpenAL_AllocChannels ();
				S_StopAllSounds ();	//inits freeplays
				S_StartLocalSound ("openalinit.wav");
			}
			else
			{
				Com_Printf ("OpenAL failed to initialize; no sound available\n", LOG_CLIENT);
			}
#else
			Com_Printf ("This binary was compiled without OpenAL support.\n", LOG_CLIENT);
#endif
		}
		else
		{
			if (!SNDDMA_Init(fullInit))
				return;

			Cmd_AddCommand("play", S_Play);
			Cmd_AddCommand("stopsound", S_StopAllSounds);
			Cmd_AddCommand("soundlist", S_SoundList_f);
			Cmd_AddCommand("soundinfo", S_SoundInfo_f);

			S_InitScaletable ();

			sound_started = 1;
			num_sfx = 0;

			soundtime = 0;
			paintedtime = 0;

			if (!cl_quietstartup->intvalue || developer->intvalue)
				Com_Printf ("sound sampling rate: %i\n", LOG_CLIENT|LOG_NOTICE, dma.speed);

			S_StopAllSounds ();
		}
	}

	if (!cl_quietstartup->intvalue || developer->intvalue)
		Com_Printf("------------------------------------\n", LOG_CLIENT|LOG_NOTICE);
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

#ifdef USE_OPENAL

static void S_OpenAL_FreeChannels (void)
{
	openal_channel_t	*ch;
	int					i;

	for (i = 0, ch = s_openal_channels; i < s_openal_numChannels; i++, ch++)
	{
		qalDeleteSources(1, &ch->sourceNum);
		memset(ch, 0, sizeof(*ch));
	}

	s_openal_numChannels = 0;
}

void S_OpenAL_FreeSounds (void)
{
	sfx_t	*sfx;
	int		i;

	// Stop all sounds
	S_StopAllSounds();

	// Free all sounds
	for (i = 0; i < num_sfx; i++)
	{
		sfx = &known_sfx[i];

		qalDeleteBuffers(1, &sfx->bufferNum);
	}
}

#endif

void S_Shutdown(void)
{
	int		i;
	sfx_t	*sfx;

	if (!sound_started)
		return;

#ifdef USE_OPENAL
	if (openal_active)
	{
		S_OpenAL_FreeSounds ();
		S_OpenAL_FreeChannels ();
	}
#endif

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

#ifdef USE_OPENAL
	if (openal_active)
	{
		ALimp_Shutdown ();
	}
	else
#endif
	{
		SNDDMA_Shutdown();
		Cmd_RemoveCommand("soundlist");
		Cmd_RemoveCommand("soundinfo");
	}
	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("stopsound");
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

#ifdef USE_OPENAL
	sfx->loaded = false;
	sfx->samples = 0;
	sfx->rate = 0;
	sfx->format = 0;
	sfx->bufferNum = 0;
#endif

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

#ifdef USE_OPENAL
	sfx->loaded = false;
	sfx->samples = 0;
	sfx->rate = 0;
	sfx->format = 0;
	sfx->bufferNum = 0;
#endif

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
	{
#ifdef USE_OPENAL
		if (openal_active)
		{
			S_OpenAL_LoadSound (sfx);
		}
		else
#endif
		{
			S_LoadSound (sfx);
		}
	}

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
			if (sfx->truename)
				Z_Free (sfx->truename); // memleak fix from echon
			rbdelete (sfx->name, knownsounds);
			sfx->cache = NULL;
			sfx->name[0] = 0;
#ifdef USE_OPENAL
			sfx->loaded = false;
#endif
			//memset (sfx, 0, sizeof(*sfx));
		}
		else
		{	// make sure it is paged in
#ifdef USE_OPENAL
			if (openal_active)
				continue;
#endif

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

#ifdef USE_OPENAL
		if (openal_active)
			S_OpenAL_LoadSound (sfx);
		else
#endif
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

	if (entchannel < 0)
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
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		rscale = 0.5f * (1.0f + dot);
		lscale = 0.5f * (1.0f - dot);
	}

	// add in distance effect
	scale = (1.0f - dist) * rscale;
	*right_vol = (int) (master_vol * scale);
	if (*right_vol < 0)
		*right_vol = 0;

	scale = (1.0f - dist) * lscale;
	*left_vol = (int) (master_vol * scale);
	if (*left_vol < 0)
		*left_vol = 0;
}

void Snd_GetEntityOrigin (int entNum, vec3_t origin)
{
	if (entNum == cl.playernum + 1)
		FastVectorCopy (listener_origin, *origin);
	else
		CL_GetEntityOrigin (entNum, origin);
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
		FastVectorCopy (ch->origin, origin);
	}
	else
		Snd_GetEntityOrigin (ch->entnum, origin);

	S_SpatializeOrigin (origin, (float)ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
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
		ch->dist_mult = ps->attenuation * 0.001f;
	else
		ch->dist_mult = ps->attenuation * 0.0005f;
	ch->master_vol = (int)ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	FastVectorCopy (ps->origin, ch->origin);
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
				p[0] = 0;
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
#ifdef USE_OPENAL
void S_OpenAL_StartSound (const vec3_t position, int entNum, int entChannel, sfx_t *sfx, float volume, float attenuation, int timeOfs)
{
	playsound_t *ps, *sort;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (sfx->name[0] == '*')
		sfx = S_RegisterSexedSound(sfx->name, entNum);

	// Make sure the sound is loaded
	if (!S_OpenAL_LoadSound(sfx))
		return;

	// Allocate a playSound
	ps = S_AllocPlaysound ();
	if (!ps)
	{
		if (sfx->name[0] == '#')
			Com_DPrintf("Dropped sound %s\n", &sfx->name[1]);
		else
			Com_DPrintf("Dropped sound sound/%s\n", sfx->name);

		return;
	}

	volume *= volume;

	ps->sfx = sfx;
	ps->entnum = entNum;
	ps->entchannel = entChannel;

	if (position)
	{
		ps->fixed_origin = true;
		FastVectorCopy (*position, ps->origin);
	}
	else
		ps->fixed_origin = false;

	ps->volume = volume;
	ps->attenuation = attenuation;
	ps->begin = cl.time + timeOfs;

	// Sort into the pending playSounds list
	for (sort = s_pendingplays.next; sort != &s_pendingplays&& sort->begin < ps->begin; sort = sort->next)
		;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}
#endif

void S_StartSound(vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs)
{
	sfxcache_t	*sc;
	int			vol;
	playsound_t	*ps, *sort;
	int			start;

#ifdef USE_OPENAL
	if (openal_active)
	{
		S_OpenAL_StartSound (origin, entnum, entchannel, sfx, fvol, attenuation, (int)(timeofs * 1000.0));
		return;
	}
#endif

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

	vol = (int)(fvol*255);

	// make the playsound_t
	ps = S_AllocPlaysound ();
	if (!ps)
		return;

	if (origin)
	{
		FastVectorCopy (*origin, ps->origin);
		ps->fixed_origin = true;
	}
	else
		ps->fixed_origin = false;

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->sfx = sfx;

	// drift s_beginofs
	start = (int)(cl.frame.servertime * 0.001f * dma.speed + s_beginofs);
	if (start < paintedtime)
	{
		start = paintedtime;
		s_beginofs = (int)(start - (cl.frame.servertime * 0.001f * dma.speed));
	}
	else if (start > paintedtime + 0.3f * dma.speed)
	{
		start = (int)(paintedtime + 0.1f * dma.speed);
		s_beginofs = (int)(start - (cl.frame.servertime * 0.001f * dma.speed));
	}
	else
	{
		s_beginofs-=10;
	}

	if (!timeofs)
		ps->begin = paintedtime;
	else
		ps->begin = (int)(start + timeofs * dma.speed);

	ps->volume = fvol*255;

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
	S_StartSound (NULL, cl.playernum + 1, 0, sfx, 1, ATTN_NONE, 0);
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

#ifdef USE_OPENAL
static void S_OpenAL_StopChannel (openal_channel_t *ch)
{
	ch->sfx = NULL;

	qalSourceStop(ch->sourceNum);
	qalSourcei(ch->sourceNum, AL_BUFFER, 0);
}
#endif

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

#ifdef USE_OPENAL
	if (openal_active)
	{
		openal_channel_t	*ch;

		// Stop all the channels
		for (i = 0, ch = s_openal_channels; i < s_openal_numChannels; i++, ch++)
		{
			if (!ch->sfx)
				continue;

			S_OpenAL_StopChannel(ch);
		}

		// Reset frame count
		s_openal_frameCount = 0;
	}
#endif

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
		/*if (ent->solid == 31)
		{
			cmodel_t	*model;

			model = cl.model_clip[ent->modelindex];

			//not loaded (deferred?)
			if (!model)
				continue;

			origin[0] = ent->origin[0]+0.5f*(model->mins[0]+model->maxs[0]);
			origin[1] = ent->origin[1]+0.5f*(model->mins[1]+model->maxs[1]);
			origin[2] = ent->origin[2]+0.5f*(model->mins[2]+model->maxs[2]);
		}
		else
		{
			VectorCopy (ent->origin, origin);
		}*/
		Snd_GetEntityOrigin (ent->number, origin);

		// find the total contribution of all sounds of this type
		S_SpatializeOrigin (origin, 255.0f, SOUND_LOOPATTENUATE,
			&left_total, &right_total);

		for (j=i+1 ; j<cl.frame.num_entities ; j++)
		{
			if (sounds[j] != sounds[i])
				continue;
			sounds[j] = 0;	// don't check this again later

			num = (cl.frame.parse_entities + j)&(MAX_PARSE_ENTITIES-1);
			ent = &cl_parse_entities[num];

			S_SpatializeOrigin (ent->origin, 255.0f, SOUND_LOOPATTENUATE, 
				&left, &right);
			left_total += left;
			right_total += right;
		}

		if (left_total == 0 && right_total == 0)
			continue;		// not audible

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

	if (!sound_started)
		return;

#ifdef USE_OPENAL
	if (openal_active)
		return;
#endif

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;
	scale = (float)rate / dma.speed;

//Com_Printf ("%i < %i < %i\n", soundtime, paintedtime, s_rawend);
	if (channels == 2 && width == 2)
	{
		if (scale == 1.0f)
		{	// optimized case
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    LittleShort(((int16 *)data)[i*2]) << 8;
				s_rawsamples[dst].right =
				    LittleShort(((int16 *)data)[i*2+1]) << 8;
			}
		}
		else
		{
			for (i=0 ; ; i++)
			{
				src = (int)(i*scale);
				if (src >= samples)
					break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    LittleShort(((int16 *)data)[src*2]) << 8;
				s_rawsamples[dst].right =
				    LittleShort(((int16 *)data)[src*2+1]) << 8;
			}
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = (int)(i*scale);
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    LittleShort(((int16 *)data)[src]) << 8;
			s_rawsamples[dst].right =
			    LittleShort(((int16 *)data)[src]) << 8;
		}
	}
	else if (channels == 2 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = (int)(i*scale);
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
			src = (int)(i*scale);
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

/*
 =================
 S_FreeChannels
 =================
*/
#ifdef USE_OPENAL
static void S_OpenAL_PlayChannel (openal_channel_t *ch, sfx_t *sfx)
{
	ch->sfx = sfx;

	qalSourcei(ch->sourceNum, AL_BUFFER, sfx->bufferNum);
	qalSourcei(ch->sourceNum, AL_LOOPING, ch->loopSound);
	qalSourcei(ch->sourceNum, AL_SOURCE_RELATIVE, AL_FALSE);
	qalSourcePlay(ch->sourceNum);
}

openal_channel_t *S_OpenAL_PickChannel (int entNum, int entChannel)
{
	openal_channel_t	*ch;
	int					i;
	int					firstToDie = -1;
	int					oldestTime = cl.time;

	if (entNum < 0 || entChannel < 0)
		Com_Error (ERR_DROP, "S_PickChannel: bad entNum");

	for (i = 0, ch = s_openal_channels; i < s_openal_numChannels; i++, ch++)
	{
		// Don't let game sounds override streaming sounds
		if (ch->streaming)
			continue;

		// Check if this channel is active
		if (!ch->sfx)
		{
			// Free channel
			firstToDie = i;
			break;
		}

		// Channel 0 never overrides
		if (entChannel != 0 && (ch->entNum == entNum && ch->entChannel == entChannel))
		{
			// Always override sound from same entity
			firstToDie = i;
			break;
		}

		// Don't let monster sounds override player sounds
		if (entNum != cl.playernum+1 && ch->entNum == cl.playernum+1)
			continue;

		// Replace the oldest sound
		if (ch->startTime < oldestTime)
		{
			oldestTime = ch->startTime;
			firstToDie = i;
		}
	}

	if (firstToDie == -1)
		return NULL;

	ch = &s_openal_channels[firstToDie];

	ch->entNum = entNum;
	ch->entChannel = entChannel;
	ch->startTime = cl.time;

	// Make sure this channel is stopped
	qalSourceStop(ch->sourceNum);
	qalSourcei(ch->sourceNum, AL_BUFFER, 0);

	return ch;
}

static void S_OpenAL_SpatializeChannel (openal_channel_t *ch)
{
	vec3_t	position;

	// Update position and velocity
	if (ch->entNum == cl.playernum+1 || !ch->distanceMult)
	{
		qalSourcefv(ch->sourceNum, AL_POSITION, s_openal_listener.position);
		//qalSourcefv(ch->sourceNum, AL_VELOCITY, s_openal_listener.velocity);
	}
	else
	{
		if (ch->fixedPosition)
		{
			qalSource3f(ch->sourceNum, AL_POSITION, ch->position[1], ch->position[2], -ch->position[0]);
			//qalSource3f(ch->sourceNum, AL_VELOCITY, 0, 0, 0);
		}
		else
		{
			if (ch->loopSound)
				Snd_GetEntityOrigin (ch->loopNum, position);
			else
				Snd_GetEntityOrigin (ch->entNum, position);

			qalSource3f(ch->sourceNum, AL_POSITION, position[1], position[2], -position[0]);
			//qalSource3f(ch->sourceNum, AL_VELOCITY, velocity[1], velocity[2], -velocity[0]);
		}
	}

	// Update min/max distance
	if (ch->distanceMult)
		qalSourcef(ch->sourceNum, AL_REFERENCE_DISTANCE, 240.0f * ch->distanceMult);
	else
		qalSourcef(ch->sourceNum, AL_REFERENCE_DISTANCE,  8192);

	qalSourcef(ch->sourceNum, AL_MAX_DISTANCE, 8192);

	// Update volume and rolloff factor
	qalSourcef(ch->sourceNum, AL_GAIN, s_openal_volume->value * ch->volume);

	qalSourcef(ch->sourceNum, AL_ROLLOFF_FACTOR, 1.0f);
}

static void S_OpenAL_AddLoopingSounds (void)
{
	entity_state_t			*ent;
	sfx_t					*sfx;
//	openal_sfx_t			asfx;
	openal_channel_t		*ch;
	int						i, j;

	if (cls.state != ca_active || cl_paused->intvalue)
		return;

	for (i = 0; i < cl.frame.num_entities; i++)
	{
		ent = &cl_parse_entities[(cl.frame.parse_entities+i) & (MAX_PARSE_ENTITIES-1)];
		if (!ent->sound)
			continue;

		sfx = cl.sound_precache[ent->sound];

		if (!sfx || !sfx->loaded)
			continue;		// Bad sound effect

		// If this entity is already playing the same sound effect on an
		// active channel, then simply update it
		for (j = 0, ch = s_openal_channels; j < s_openal_numChannels; j++, ch++)
		{
			if (ch->sfx != sfx)
				continue;

			if (!ch->loopSound)
				continue;

			if (ch->loopNum != ent->number)
				continue;

			if (ch->loopFrame + 1 != s_openal_frameCount)
				continue;

			ch->loopFrame = s_openal_frameCount;
			break;
		}

		if (j != s_openal_numChannels)
			continue;

		// Otherwise pick a channel and start the sound effect
		ch = S_OpenAL_PickChannel(0, 0);

		if (!ch)
		{
			if (sfx->name[0] == '#')
				Com_DPrintf("Dropped sound %s\n", &sfx->name[1]);
			else
				Com_DPrintf("Dropped sound sound/%s\n", sfx->name);

			continue;
		}

		ch->loopSound = true;
		ch->loopNum = ent->number;
		ch->loopFrame = s_openal_frameCount;
		ch->fixedPosition = false;
		ch->volume = 1.0f;
		ch->distanceMult = 0.3f;//1.0f / ATTN_STATIC;

		S_OpenAL_SpatializeChannel(ch);

		S_OpenAL_PlayChannel(ch, sfx);
	}
}

static int S_OpenAL_ChannelState (openal_channel_t *ch)
{
	int		state;

	qalGetSourcei(ch->sourceNum, AL_SOURCE_STATE, &state);

	return state;
}

static void S_OpenAL_IssuePlaySounds (void)
{
	playsound_t 		*ps;
	openal_channel_t	*ch;

	for (;;)
	{
		ps = s_pendingplays.next;

		if (ps == &s_pendingplays)
			break;		// No more pending playSounds

		if (ps->begin > cl.time)
			break;		// No more pending playSounds this frame

		// Pick a channel and start the sound effect
		ch = S_OpenAL_PickChannel(ps->entnum, ps->entchannel);

		if (!ch)
		{
			if (ps->sfx->name[0] == '#')
				Com_DPrintf("Dropped sound %s\n", &ps->sfx->name[1]);
			else
				Com_DPrintf("Dropped sound sound/%s\n", ps->sfx->name);

			S_FreePlaysound (ps);
			continue;
		}

		ch->loopSound = false;
		ch->fixedPosition = ps->fixed_origin;
		FastVectorCopy (ps->origin, ch->position);
		ch->volume = ps->volume;

		if (ps->attenuation != ATTN_NONE)
			ch->distanceMult = 1.0f / ps->attenuation;
		else
			ch->distanceMult = 0.0f;

		S_OpenAL_SpatializeChannel(ch);

		S_OpenAL_PlayChannel(ch, ps->sfx);

		// Free the playSound
		S_FreePlaysound(ps);
	}
}

//=============================================================================
int	EXPORT CL_PMpointcontents (vec3_t point);
void S_Update_OpenAL (vec3_t position, const vec3_t velocity, const vec3_t at, const vec3_t up)
{
	unsigned			eaxEnv;
	openal_channel_t	*ch;
	int					i, total = 0;

	// Bump frame count
	s_openal_frameCount++;

	// Set up listener
	VectorSet(s_openal_listener.position, position[1], position[2], -position[0]);
	//VectorSet(s_openal_listener.velocity, velocity[1], velocity[2], -velocity[0]);

	s_openal_listener.orientation[0] = at[1];
	s_openal_listener.orientation[1] = -at[2];
	s_openal_listener.orientation[2] = -at[0];
	s_openal_listener.orientation[3] = up[1];
	s_openal_listener.orientation[4] = -up[2];
	s_openal_listener.orientation[5] = -up[0];

	qalListenerfv(AL_POSITION, s_openal_listener.position);
	//qalListenerfv(AL_VELOCITY, s_openal_listener.velocity);
	qalListenerfv(AL_ORIENTATION, s_openal_listener.orientation);
#ifdef _WIN32
	qalListenerf(AL_GAIN, (ActiveApp) ? s_volume->value : 0);
#else
	qalListenerf(AL_GAIN, s_volume->value);
#endif

	// Set state
	qalDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	//qalDistanceModel(AL_INVERSE_DISTANCE);

	//qalDopplerFactor(s_dopplerFactor->value);
	//qalDopplerVelocity(s_dopplerVelocity->value);

	// If EAX is enabled, apply listener environmental effects
#ifdef _WIN32
	if (alConfig.eax)
	{
		if (cls.state != ca_active)
		{
			eaxEnv = EAX_ENVIRONMENT_GENERIC;
		}
		else
		{
			if (CL_PMpointcontents (position) & MASK_WATER)
				eaxEnv = EAX_ENVIRONMENT_UNDERWATER;
			else
				eaxEnv = EAX_ENVIRONMENT_GENERIC;
		}

		if (eaxEnv != alConfig.eaxState)
		{
			alConfig.eaxState = eaxEnv;
			qalEAXSet (&DSPROPSETID_EAX20_ListenerProperties, DSPROPERTY_EAXLISTENER_ENVIRONMENT | DSPROPERTY_EAXLISTENER_IMMEDIATE, 0, &eaxEnv, sizeof(eaxEnv));
		}
	}
#endif

	// Stream background track
	//S_StreamBackgroundTrack();

	// Add looping sounds
	if (s_ambient->intvalue)
		S_OpenAL_AddLoopingSounds();

	// Issue playSounds
	S_OpenAL_IssuePlaySounds();

	// Update spatialization for all sounds
	for (i = 0, ch = s_openal_channels; i < s_openal_numChannels; i++, ch++)
	{
		if (!ch->sfx)
			continue;		// Not active

		// Check for stop
		if (ch->loopSound)
		{
			if (ch->loopFrame != s_openal_frameCount)
			{
				S_OpenAL_StopChannel(ch);
				continue;
			}
		}
		else
		{
			if (S_OpenAL_ChannelState(ch) == AL_STOPPED)
			{
				S_OpenAL_StopChannel(ch);
				continue;
			}
		}

		// Respatialize channel
		S_OpenAL_SpatializeChannel(ch);

		if (s_show->intvalue)
		{
			if (ch->sfx->name[0] == '#')
				Com_Printf("%2i: %s\n", LOG_CLIENT, i+1, &ch->sfx->name[1]);
			else
				Com_Printf("%2i: sound/%s\n", LOG_CLIENT, i+1, ch->sfx->name);
		}

		total++;
	}

	if (s_show->intvalue)
		Com_Printf("--- ( %i ) ---\n", LOG_CLIENT, total);
}
#endif

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

#ifdef USE_OPENAL
	if (openal_active)
	{
		S_Update_OpenAL (origin, vec3_origin, forward, up);
		return;
	}
#endif

	FastVectorCopy (*origin, listener_origin);
	FastVectorCopy (*forward, listener_forward);
	FastVectorCopy (*right, listener_right);
	FastVectorCopy (*up, listener_up);

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
	uint32	endtime;
	int32	samps;

	if (!sound_started)
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
	endtime = (int)(soundtime + s_mixahead->value * dma.speed);
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
			Q_strncpy (name, Cmd_Argv(i), sizeof(name)-5);
			strcat(name, ".wav");
		}
		else
			Q_strncpy (name, Cmd_Argv(i), sizeof(name)-1);

		if (strstr(name, "..") || name[0] == '/' || name[0] == '\\') {
			Com_Printf ("Bad filename %s\n", LOG_CLIENT, name);
			return;
		}

		sfx = S_RegisterSound(name);
		if (sfx)
		{
			S_StartSound(NULL, cl.playernum+1, 0, sfx, 1.0, 1.0, 0);
			//sfx->name[0] = '\0';
		}
		i++;
	}
}

void S_SoundList_f (void)
{
	int		i;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total;
	int		numsounds;

	total = 0;
	numsounds = 0;

	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (!sfx->name[0])
			continue;

		sc = sfx->cache;
		if (sc)
		{
			size = sc->length*sc->width*(sc->stereo+1);
			total += size;
			Com_Printf("%s(%2db) %8i : %s\n", LOG_CLIENT, sc->loopstart != -1 ? "L" : " ", sc->width*8,  size, sfx->name);
		}
		else
		{
			if (sfx->name[0] == '*')
				Com_Printf("    placeholder : %s\n", LOG_CLIENT, sfx->name);
			else
				Com_Printf("    not loaded  : %s\n", LOG_CLIENT, sfx->name);
		}
		numsounds++;
	}
	Com_Printf ("Total resident: %i bytes (%.2f MB) in %d sounds\n", LOG_CLIENT, total, (float)total / 1024 / 1024, numsounds);
}

