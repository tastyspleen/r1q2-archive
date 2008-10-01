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

#ifndef _QSHARED_H

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifndef NO_ZLIB
#ifdef _WIN32
#define ZEXPORT __cdecl
#endif
#include <zlib.h>
#ifdef _WIN32
#undef ZEXPORT
#endif
#endif

//for broken Makefiles?
#ifdef __linux__
#ifndef LINUX
#define LINUX 1
#endif
#endif

// q_shared.h -- included first by ALL program modules

#ifdef _WIN32
// unknown pragmas are SUPPOSED to be ignored, but....
//#pragma warning(disable : 4244)     // MIPS
//#pragma warning(disable : 4136)     // X86
//#pragma warning(disable : 4051)     // ALPHA

//#pragma warning(disable : 4244)		// truncation
#pragma warning(disable : 4018)     // signed/unsigned mismatch
//#pragma warning(disable : 4305)		// truncation from const double to float

#pragma warning(disable : 4096)		// __cdecl must be used with '...'*/
#pragma warning(disable : 4142)		//benign redefinition

#pragma warning(2 : 4189 4210 4389)	// useful warnings

//"off by default" warnings
#pragma warning(3 : 4056)
#pragma warning(3 : 4191)
#pragma warning(3 : 4254)
#pragma warning(3 : 4287)
#pragma warning(3 : 4296)
#pragma warning(3 : 4514)
#pragma warning(3 : 4545)
#pragma warning(3 : 4546)
#pragma warning(3 : 4547)
#pragma warning(3 : 4548)
#pragma warning(3 : 4549)
#pragma warning(3 : 4555)
#pragma warning(3 : 4905)
#pragma warning(3 : 4906)
#pragma warning(3 : 4245)
//#pragma warning(disable: 4996)		// deprecated functions

#pragma intrinsic(memcmp)
//#pragma intrinsic(memset)

#if _MSC_VER >= 1400
	#define NORETURN __declspec(noreturn)
	#define RESTRICT __declspec(restrict)
	#define NOALIAS __declspec(noalias)
#else
	#define RESTRICT
	#define NORETURN
	#define NOALIAS
	#define sqrtf (float)sqrt
	#define ceilf (float)ceil
#endif

#define alloca _alloca
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define	Q_strlwr _strlwr
//#define Q_snprintf _snprintf
#define Q_vsnprintf _vsnprintf
#ifndef _M_AMD64
	#define Q_stricmp _strcmpi	//odd, amd64 libc is missing this...
	#define Q_strncasecmp _strnicmp
#endif
#define strdup _strdup
#ifndef fileno
#define fileno _fileno
#endif
#define strlwr _strlwr
#define filelength _filelength
#define stricmp _stricmp
#define putenv _putenv
#define EXPORT __cdecl
#define IMPORT __cdecl
//#if !defined _M_AMD64
 //#define DEBUGBREAKPOINT __asm int 3
//#else
 //#define DEBUGBREAKPOINT DebugBreak
//#endif
#define __attribute__(x) 
#define PACKED_STRUCT
typedef __int32 int32;
typedef __int16 int16;
typedef __int64 int64;
typedef unsigned __int32 uint32;
typedef unsigned __int16 uint16;
typedef unsigned __int64 uint64;
#else /* NON-WIN32 */
#include <stdint.h>
#define WINAPI
#define RESTRICT
#define NORETURN
#define NOALIAS
typedef int32_t int32;
typedef int16_t int16;
typedef int64_t int64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint64_t uint64;
#define PACKED_STRUCT __attribute__((packed))
//XXX: are these portable enough on non-win32?
#define Q_stricmp strcasecmp
#define Q_strncasecmp strncasecmp
#define EXPORT
#define IMPORT
void Q_strlwr (char *str);
int Q_vsnprintf (char *buff, size_t len, const char *fmt, va_list va);
//int Q_snprintf (char *buff, size_t len, const char *fmt, ...);

#endif

#if (defined _M_IX86 || defined __i386__) && !defined C_ONLY && !defined __sun__
#define id386	1
#else
#define id386	0
#endif

#if defined _M_ALPHA && !defined C_ONLY
#define idaxp	1
#else
#define idaxp	0
#endif

#ifdef  NDEBUG
#define Q_assert(exp)     ((void)0)
#else
void _Q_assert (char *expression, char *function, uint32 line);
#define Q_assert(exp) (void)( (exp) || (_Q_assert(#exp, __FILE__, __LINE__), 0) )
#endif

typedef unsigned char 		byte;
typedef enum {false, true}	qboolean;

//r1: set this to 1 if you have a stupid endian thingy
#define Q_BIGENDIAN 0

//#define random()	(randomMT() / ((float)0xFFFFFFFFU))

//32 bit float precision caps this to 0.00000000023283064f which gives max result of .99999998407391880
#define	random()	((randomMT() * 0.00000000023283064f))


#define	frand()		(random())
#define	crand()		(((int)randomMT() - 0x7FFFFFFF) * 0.000000000465661287307739257812f)

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifdef _WIN32
#define FLOAT2INTCAST(f)(*((int32 *)(&f)))
#define FLOAT2UINTCAST(f)(*((uint32 *)(&f)))
#define FLOAT_LT_ZERO(f) (FLOAT2UINTCAST(f) > 0x80000000U)
#define FLOAT_LE_ZERO(f) (FLOAT2INTCAST(f) <= 0)
#define FLOAT_GT_ZERO(f) (FLOAT2INTCAST(f) > 0)
#define FLOAT_GE_ZERO(f) (FLOAT2UINTCAST(f) <= 0x80000000U)
#define	FLOAT_EQ_ZERO(f) (FLOAT2INTCAST(f) == 0)
#define	FLOAT_NE_ZERO(f) (FLOAT2INTCAST(f) != 0)
#else
//gcc breaks ieee compatibility with -ffast-math? i guess since these break horribly on linux
#define	FLOAT_LT_ZERO(f) ((f) < 0)
#define FLOAT_LE_ZERO(f) ((f) <= 0)
#define FLOAT_GT_ZERO(f) ((f) > 0)
#define FLOAT_GE_ZERO(f) ((f) >= 0)
#define	FLOAT_EQ_ZERO(f) ((f) == 0)
#define	FLOAT_NE_ZERO(f) ((f) != 0)
#endif

//terminating strncpy
#define Q_strncpy(dst, src, len) \
do { \
	strncpy ((dst), (src), (len)); \
	(dst)[(len)] = 0; \
} while (0)

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	80		// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		512		// max length of an individual token

#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			128		// max length of a filesystem pathname

//
// per-level limits
//
#define	MAX_CLIENTS			256		// absolute limit
#define	MAX_EDICTS			1024	// must change protocol to increase more
#define	MAX_LIGHTSTYLES		256
#define	MAX_MODELS			256		// these are sent over the net as bytes
#define	MAX_SOUNDS			256		// so they cannot be blindly increased
#define	MAX_IMAGES			256
#define	MAX_ITEMS			256
#define MAX_GENERAL			(MAX_CLIENTS*2)	// general config strings


// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages

#define	ERR_FATAL		0		// exit the entire game with a popup window
#define	ERR_DROP		1		// print to console and disconnect from game
#define	ERR_DISCONNECT	2		// not an error, just a normal exit
#define	ERR_GAME		3		// r1ch: game dll error, allow special handling
#define	ERR_NET			4		// r1ch: network error, don't use net functions after seeing
#define	ERR_DIE			5		// r1ch: die ASAP. memory corrupt or other super-fatal
#define	ERR_HARD		6		// r1ch: don't try to auto restart from this error

#define	PRINT_ALL			0
#define PRINT_DEVELOPER		1		// only print when "developer 1"
#define PRINT_ALERT			2		


// destination class for gi.multicast()
typedef enum
{
MULTICAST_ALL,
MULTICAST_PHS,
MULTICAST_PVS,
MULTICAST_ALL_R,
MULTICAST_PHS_R,
MULTICAST_PVS_R
} multicast_t;

int16 ShortSwap (int16 l);
int32 LongSwap (int32 l);

#if !Q_BIGENDIAN
#define LittleShort(l) (l)
#define LittleLong(l) (l)
#define LittleFloat(l) (l)
#endif

/*
==============================================================

MATHLIB

==============================================================
*/

typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI			3.14159265358979323846F		// matches value in gcc v2 math.h
#endif

#define	M_PI_DIV_2		1.570796326794896619230F
#define	M_PI2_DIV_360	0.01745329251994329576922F

struct cplane_s;

extern vec3_t vec3_origin;

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

// microsoft's fabs seems to be ungodly slow...
//float Q_fabs (float f);
//#define	fabs(f) Q_fabs(f)
//extern int sse2_enabled;
#if !defined C_ONLY && !defined __linux__ && !defined __sgi && !defined SSE2 && !defined __FreeBSD__
//extern void __cdecl Q_sseinit (void);
void __cdecl Q_ftol2( float f, int *out );
int __cdecl Q_ftol( float f );
extern void __cdecl Q_fastfloats (float *f, int *out);
//extern void __cdecl Q_ftolsse( float f, int *out );
//the overhead of using function pointer offsets any savings of using sse2 :/
#else
int Q_ftol( float f);
void Q_fastfloats(float *f, int *out);
void Q_ftol2( float f, int *out );
#endif

//this is a function instead of a macro for the asm since some compilers are scared by inline asm
//and disable some optimizations.
void _Q_DEBUGBREAKPOINT (void);

typedef union
{
	int		i[3];
	float	f[3];
} vectorhack_t;

#define DotProduct(x,y)			(x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c)	(c[0]=a[0]-b[0],c[1]=a[1]-b[1],c[2]=a[2]-b[2])
#define VectorAdd(a,b,c)		(c[0]=a[0]+b[0],c[1]=a[1]+b[1],c[2]=a[2]+b[2])
#define VectorCopy(src,dst)		(dst[0]=src[0],dst[1]=src[1],dst[2]=src[2])
#define FastVectorCopy(src,dst)		*(vectorhack_t *)&(dst) = *(vectorhack_t *)&(src)
//#define VectorClear(a)			(a[0]=a[1]=a[2]=0)
//#define VectorClear(a)			(memset ((&a), 0, sizeof((a))))
#define VectorClear(a)			*(int *)&(a)[0] = 0, *(int *)&(a)[1] = 0, *(int *)&(a)[2] = 0
#define VectorNegate(a,b)		(b[0]=-a[0],b[1]=-a[1],b[2]=-a[2])
#define VectorSet(v, x, y, z)	(v[0]=(x), v[1]=(y), v[2]=(z))
#define VectorAverage(a,b,o)	((o)[0]=((a)[0]+(b)[0])*0.5f,(o)[1]=((a)[1]+(b)[1])*0.5f,(o)[2]=((a)[2]+(b)[2])*0.5f)

//r1: macorize
#define VectorCompare(v1,v2)	(v1[0]==v2[0] && v1[1]==v2[1] && v1[2]== v2[2])
#define CrossProduct(v1,v2,c)	(c[0]=v1[1]*v2[2]-v1[2]*v2[1],c[1]=v1[2]*v2[0]-v1[0]*v2[2],c[2]=v1[0]*v2[1]-v1[1]*v2[0])
#define VectorInverse(v)		(v[0]=-v[0],v[1]=-v[1],v[2]=-v[2])
#define VectorScale(in,s,out)	(out[0]=in[0]*(float)(s),out[1]=in[1]*(float)(s),out[2]=in[2]*(float)(s))
#define VectorMA(a,s,b,c)		(c[0]=a[0]+(float)(s)*b[0],c[1]=a[1]+(float)(s)*b[1],c[2]=a[2]+(float)(s)*b[2])
#define ClearBounds(mins,maxs)	(mins[0]=mins[1]=mins[2]=99999,maxs[0]=maxs[1]=maxs[2]=-99999)

//performs comparison on encoded byte differences - pointless sending 0.00 -> 0.01 if both end up as 0 on net.
#define Vec_ByteCompare(v1,v2) \
	((int)(v1[0]*4)==(int)(v2[0]*4) && \
	(int)(v1[1]*4)==(int)(v2[1]*4) && \
	(int)(v1[2]*4) == (int)(v2[2]*4))

#define Vec_RoughCompare(v1,v2) \
	(*(int *)&(v1[0])== *(int *)&(v2[0]) && \
	*(int *)&(v1[1]) == *(int *)&(v2[1]) && \
	*(int *)&(v1[2]) == *(int *)&(v2[2]))

#define Float_ByteCompare(v1,v2) \
	((int)((v1)*8)==((int)((v2)*8)))

#define Float_RoughCompare(v1,v2) \
	(*(int *)&(v1) == *(int *)&(v2))

#define Float_AngleCompare(v1,v2) \
	(((int)((v1)*256/360) & 255) == ((int)((v2)*256/360) & 255))

//to keep ms vc auto complete happy
#ifdef UNDEFINED
void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross);
int VectorCompare (vec3_t v1, vec3_t v2);
void VectorInverse (vec3_t v);
void VectorScale (vec3_t in, vec_t scale, vec3_t /*@out@*/out);
void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t /*@out@*/vecc);
void ClearBounds (vec3_t mins, vec3_t maxs);
#endif

//void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc);

// just in case you do't want to use the macros
vec_t _DotProduct (vec3_t v1, vec3_t v2);
void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out);
void _VectorCopy (vec3_t in, vec3_t out);

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs);


vec_t VectorLength (vec3_t v);
vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2 (vec3_t v, vec3_t /*@out@*/out);

int Q_log2(int val);

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void AngleVectors (vec3_t angles, vec3_t /*@out@*//*@null@*/forward, vec3_t /*@out@*//*@null@*/right, vec3_t /*@out@*//*@null@*/up);
int EXPORT BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);
float	anglemod(float a);
float LerpAngle (float a1, float a2, float frac);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)	\
	(((p)->type < 3)?						\
	(										\
		((p)->dist <= (emins)[(p)->type])?	\
			1								\
		:									\
		(									\
			((p)->dist >= (emaxs)[(p)->type])?\
				2							\
			:								\
				3							\
		)									\
	)										\
	:										\
		BoxOnPlaneSide( (emins), (emaxs), (p)))

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );


//=============================================

char *COM_SkipPath (char *pathname);
void COM_StripExtension (const char *in, char *out);
void COM_FileBase (char *in, char *out);
void COM_FilePath (const char *in, char *out);
void COM_DefaultExtension (char *path, const char *extension);

const char *COM_Parse (char **data_p);
// data is an in/out parm, returns a parsed out token

int Com_sprintf (char /*@out@*/*dest, int size, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

void Com_PageInMemory (byte *buffer, int size);

int wildcardfit (char *wildcard, char *test);
/* this function implements the UN*X wildcards and returns  */
/* 0  if *wildcard does not match *test                     */
/* 1  if *wildcard matches *test                            */

#ifdef ANTICHEAT
int Sys_GetAntiCheatAPI (void);
#endif

//=============================================

// portable case insensitive compare
#ifndef Q_stricmp
int Q_stricmp (const char *s1, const char *s2);
#endif

#ifndef Q_strncasecmp
int Q_strncasecmp (const char *s1, const char *s2, size_t n);
#endif

//=============================================

#if Q_BIGENDIAN
int16	LittleShort(int16 l);
int32		LittleLong (int32 l);
float	LittleFloat (float l);
#endif

void	Swap_Init (void);
char	*va(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

//=============================================

//
// key / value info strings
//
#define	MAX_INFO_KEY		64
#define	MAX_INFO_VALUE		64
#define	MAX_INFO_STRING		512

qboolean Info_KeyExists (const char *s, const char *key);
char *Info_ValueForKey (const char *s, const char *key);
void Info_RemoveKey (char *s, const char *key);
void Info_SetValueForKey (char *s, const char *key, const char *value);
qboolean Info_Validate (const char *s);
qboolean Info_CheckBytes (const char *s);

void seedMT (uint32 seed);
uint32 randomMT (void);

/*
==============================================================

SYSTEM SPECIFIC

==============================================================
*/

extern	unsigned int	curtime;		// time returned by last Sys_Milliseconds

unsigned int		Sys_Milliseconds (void);
void	Sys_Mkdir (char *path);
void	Sys_DebugBreak (void);

// large block stack allocation routines
void	*Hunk_Begin (int maxsize, int precommit);
void	*Hunk_Alloc (int size);
void	Hunk_Free (void *buf);
int		Hunk_End (void);

// directory searching
#define SFF_ARCH    0x01
#define SFF_HIDDEN  0x02
#define SFF_RDONLY  0x04
#define SFF_SUBDIR  0x08
#define SFF_SYSTEM  0x10

/*
** pass in an attribute mask of things you wish to REJECT
*/
char	*Sys_FindFirst (char *path, uint32 musthave, uint32 canthave );
char	*Sys_FindNext ( uint32 musthave, uint32 canthave );
void	Sys_FindClose (void);


void Sys_Sleep (int msec);

#define	LOG_GENERAL		0x0
#define LOG_CLIENT		0x1
#define	LOG_SERVER		0x2
#define LOG_DEBUG		0x4
#define	LOG_WARNING		0x8
#define	LOG_ERROR		0x10
#define	LOG_GAME		0x20
#define	LOG_CONNECT		0x40
#define	LOG_NAME		0x80
#define	LOG_DROP		0x100
#define	LOG_KICK		0x200
#define	LOG_EXPLOIT		0x400
#define	LOG_DOWNLOAD	0x800
#define	LOG_NOTICE		0x1000
#define	LOG_CHAT		0x2000
#define	LOG_NET			0x4000
#define	LOG_GAMEDEBUG	0x8000
#define LOG_ANTICHEAT	0x10000

// this is only here so the functions in q_shared.c and q_shwin.c can link
NORETURN void Sys_Error (const char *error, ...) __attribute__ ((format (printf, 1, 2)));
void Com_Printf (const char *fmt, int level, ...) __attribute__ ((format (printf, 1, 3)));


/*
==========================================================

CVARS (console variables)

==========================================================
*/

#ifndef CVAR
#define	CVAR

#define	CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define	CVAR_USERINFO	2	// added to userinfo  when changed
#define	CVAR_SERVERINFO	4	// added to serverinfo when changed
#define	CVAR_NOSET		8	// don't allow change from console at all,
							// but can be set from the command line
#define	CVAR_LATCH		16	// save changes until server restart

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s
{
	char		*name;													//0x00
	char		*string;												//0x04
	char		*latched_string;	// for CVAR_LATCH vars				//0x08
	int			flags;													//0x0c
	qboolean	modified;	// set each time the cvar is changed		//0x10
	float		value;													//0x14
	struct cvar_s *next;												//0x18

	//r1ch: added this to avoid all the if (x->modified) bloat
	void		(*changed) (struct cvar_s *self, char *oldValue, char *newValue);
	int			intvalue;
	const char	*help;
} cvar_t;

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
	char				name[MAX_ALIAS_NAME];
	struct cmdalias_s	*next;
	char				*value;
} cmdalias_t;

typedef void (*xcommand_t) (void);

typedef struct cmd_function_s
{
	const char				*name;
	struct cmd_function_s	*next;
	xcommand_t				function;
} cmd_function_t;

#endif		// CVAR

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			2		// translucent, but not watery
#define	CONTENTS_AUX			4
#define	CONTENTS_LAVA			8
#define	CONTENTS_SLIME			16
#define	CONTENTS_WATER			32
#define	CONTENTS_MIST			64
#define	LAST_VISIBLE_CONTENTS	64

// remaining contents are non-visible, and don't eat brushes

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000

// currents can be added to any other contents, and may be mixed
#define	CONTENTS_CURRENT_0		0x40000
#define	CONTENTS_CURRENT_90		0x80000
#define	CONTENTS_CURRENT_180	0x100000
#define	CONTENTS_CURRENT_270	0x200000
#define	CONTENTS_CURRENT_UP		0x400000
#define	CONTENTS_CURRENT_DOWN	0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define	CONTENTS_DEADMONSTER	0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000

#define	CONTENTS_ENT_NOCLIP		0x40000000	// r1q2 hack for non-velocity-changing clipping to solids


#define	SURF_LIGHT		0x1		// value will hold the light strength

#define	SURF_SLICK		0x2		// effects game physics

#define	SURF_SKY		0x4		// don't draw, but add to skybox
#define	SURF_WARP		0x8		// turbulent water warp
#define	SURF_TRANS33	0x10
#define	SURF_TRANS66	0x20
#define	SURF_FLOWING	0x40	// scroll towards angle
#define	SURF_NODRAW		0x80	// don't bother referencing the texture



// content masks
#define	MASK_ALL				(-1)
#define	MASK_SOLID				(CONTENTS_SOLID|CONTENTS_WINDOW)
#define	MASK_PLAYERSOLID		(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define	MASK_DEADSOLID			(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW)
#define	MASK_MONSTERSOLID		(CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
#define	MASK_WATER				(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)
#define	MASK_OPAQUE				(CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA)
#define	MASK_SHOT				(CONTENTS_SOLID|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEADMONSTER)
#define MASK_CURRENT			(CONTENTS_CURRENT_0|CONTENTS_CURRENT_90|CONTENTS_CURRENT_180|CONTENTS_CURRENT_270|CONTENTS_CURRENT_UP|CONTENTS_CURRENT_DOWN)


// gi.BoxEdicts() can return a list of either solid or trigger entities
// FIXME: eliminate AREA_ distinction?
#define	AREA_SOLID		1
#define	AREA_TRIGGERS	2


// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
} cplane_t;

//r1: "fast" plane for server calcs
typedef struct fplane_s
{
	int		type;			// for fast side tests
	float	dist;
	vec3_t	normal;
	int		signbits;		// signx + (signy<<1) + (signz<<1)
} fplane_t;

// structure offset for asm code
#define CPLANE_NORMAL_X			0
#define CPLANE_NORMAL_Y			4
#define CPLANE_NORMAL_Z			8
#define CPLANE_DIST				12
#define CPLANE_TYPE				16
#define CPLANE_SIGNBITS			17
#define CPLANE_PAD0				18
#define CPLANE_PAD1				19

typedef struct cmodel_s
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	int			headnode;
} cmodel_t;

typedef struct csurface_s
{
	char		name[16];
	int			flags;
	int			value;
} csurface_t;

typedef struct mapsurface_s  // used internally due to name len probs //ZOID
{
	csurface_t	c;
	char		rname[32];
} mapsurface_t;

// a trace is returned when a box is swept through the world
typedef struct
{
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact
	csurface_t	*surface;	// surface hit
	int			contents;	// contents on other side of surface hit
	struct edict_s	*ent;		// not set by CM_*() functions
} trace_t;



// pmove_state_t is the information necessary for client side movement
// prediction
typedef enum 
{
	// can accelerate and turn
	PM_NORMAL,
	PM_SPECTATOR,
	// no acceleration or turning
	PM_DEAD,
	PM_GIB,		// different bounding box
	PM_FREEZE
} pmtype_t;

// pmove->pm_flags
#define	PMF_DUCKED			1
#define	PMF_JUMP_HELD		2
#define	PMF_ON_GROUND		4
#define	PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define	PMF_TIME_LAND		16	// pm_time is time before rejump
#define	PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)

// this structure needs to be communicated bit-accurate
// from the server to the client to guarantee that
// prediction stays in sync, so no floats are used.
// if any part of the game code modifies this struct, it
// will result in a prediction error of some degree.
typedef struct
{
	pmtype_t	pm_type;

	int16		origin[3];		// 12.3
	int16		velocity[3];	// 12.3
	byte		pm_flags;		// ducked, jump_held, etc
	byte		pm_time;		// each unit = 8 ms
	int16		gravity;
	int16		delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters
} pmove_state_t;


//
// button bits
//
#define	BUTTON_ATTACK		1
#define	BUTTON_USE			2

//stolen for r1q2 in the name of bandwidth
#define	BUTTON_UCMD_DBLFORWARD	4
#define BUTTON_UCMD_DBLSIDE		8
#define	BUTTON_UCMD_DBLUP		16

#define BUTTON_UCMD_DBL_ANGLE1	32
#define BUTTON_UCMD_DBL_ANGLE2	64

#define	BUTTON_ANY			128			// any key whatsoever


// usercmd_t is sent to the server each client frame
typedef struct usercmd_s
{
	byte	msec;
	byte	buttons;
	int16	angles[3];
	int16	forwardmove, sidemove, upmove;
	byte	impulse;		// remove?
	byte	lightlevel;		// light level the player is standing on
} usercmd_t;


#define	MAXTOUCH	32
typedef struct
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t		cmd;
	qboolean		snapinitial;	// if s has been changed outside pmove

	// results (out)
	int			numtouch;
	struct edict_s	*touchents[MAXTOUCH];

	vec3_t		viewangles;			// clamped
	float		viewheight;

	vec3_t		mins, maxs;			// bounding box size

	struct edict_s	*groundentity;
	int			watertype;
	int			waterlevel;

	// callbacks to test the world
	trace_t		(IMPORT *trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int			(IMPORT *pointcontents) (vec3_t point);
} pmove_t;

typedef struct
{
	// state (in / out)
	pmove_state_t	s;

	// command (in)
	usercmd_t		cmd;
	qboolean		snapinitial;	// if s has been changed outside pmove

	// results (out)
	int			numtouch;
	struct edict_s	*touchents[MAXTOUCH];

	vec3_t		viewangles;			// clamped
	float		viewheight;

	vec3_t		mins, maxs;			// bounding box size

	struct edict_s	*groundentity;
	int			watertype;
	int			waterlevel;

	// callbacks to test the world
	trace_t		(IMPORT *trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);
	int			(IMPORT *pointcontents) (vec3_t point);

	float		multiplier;
	qboolean	strafehack;
	qboolean	enhanced;
} pmove_new_t;

// entity_state_t->effects
// Effects are things handled on the client side (lights, particles, frame animations)
// that happen constantly on the given entity.
// An entity that has effects will be sent to the client
// even if it has a zero index model.
#define	EF_ROTATE			0x00000001		// rotate (bonus items)
#define	EF_GIB				0x00000002		// leave a trail
#define	EF_BLASTER			0x00000008		// redlight + trail
#define	EF_ROCKET			0x00000010		// redlight + trail
#define	EF_GRENADE			0x00000020
#define	EF_HYPERBLASTER		0x00000040
#define	EF_BFG				0x00000080
#define EF_COLOR_SHELL		0x00000100
#define EF_POWERSCREEN		0x00000200
#define	EF_ANIM01			0x00000400		// automatically cycle between frames 0 and 1 at 2 hz
#define	EF_ANIM23			0x00000800		// automatically cycle between frames 2 and 3 at 2 hz
#define EF_ANIM_ALL			0x00001000		// automatically cycle through all frames at 2hz
#define EF_ANIM_ALLFAST		0x00002000		// automatically cycle through all frames at 10hz
#define	EF_FLIES			0x00004000
#define	EF_QUAD				0x00008000
#define	EF_PENT				0x00010000
#define	EF_TELEPORTER		0x00020000		// particle fountain
#define EF_FLAG1			0x00040000
#define EF_FLAG2			0x00080000
// RAFAEL
#define EF_IONRIPPER		0x00100000
#define EF_GREENGIB			0x00200000
#define	EF_BLUEHYPERBLASTER 0x00400000
#define EF_SPINNINGLIGHTS	0x00800000
#define EF_PLASMA			0x01000000
#define EF_TRAP				0x02000000

//ROGUE
#define EF_TRACKER			0x04000000
#define	EF_DOUBLE			0x08000000
#define	EF_SPHERETRANS		0x10000000
#define EF_TAGTRAIL			0x20000000
#define EF_HALF_DAMAGE		0x40000000
#define EF_TRACKERTRAIL		0x80000000
//ROGUE

// entity_state_t->renderfx flags
#define	RF_MINLIGHT			1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL		2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL		4		// only draw through eyes
#define	RF_FULLBRIGHT		8		// allways draw full intensity
#define	RF_DEPTHHACK		16		// for view weapon Z crunching
#define	RF_TRANSLUCENT		32
#define	RF_FRAMELERP		64
#define RF_BEAM				128
#define	RF_CUSTOMSKIN		256		// skin is an index in image_precache
#define	RF_GLOW				512		// pulse lighting for bonus items
#define RF_SHELL_RED		1024
#define	RF_SHELL_GREEN		2048
#define RF_SHELL_BLUE		4096

//ROGUE
#define RF_IR_VISIBLE		0x00008000		// 32768
#define	RF_SHELL_DOUBLE		0x00010000		// 65536
#define	RF_SHELL_HALF_DAM	0x00020000
#define RF_USE_DISGUISE		0x00040000
//ROGUE

// player_state_t->refdef flags
#define	RDF_UNDERWATER		1		// warp the screen as apropriate
#define RDF_NOWORLDMODEL	2		// used for player configuration screen

//ROGUE
#define	RDF_IRGOGGLES		4
#define RDF_UVGOGGLES		8
//ROGUE

//
// muzzle flashes / player effects
//
#define	MZ_BLASTER			0
#define MZ_MACHINEGUN		1
#define	MZ_SHOTGUN			2
#define	MZ_CHAINGUN1		3
#define	MZ_CHAINGUN2		4
#define	MZ_CHAINGUN3		5
#define	MZ_RAILGUN			6
#define	MZ_ROCKET			7
#define	MZ_GRENADE			8
#define	MZ_LOGIN			9
#define	MZ_LOGOUT			10
#define	MZ_RESPAWN			11
#define	MZ_BFG				12
#define	MZ_SSHOTGUN			13
#define	MZ_HYPERBLASTER		14
#define	MZ_ITEMRESPAWN		15
// RAFAEL
#define MZ_IONRIPPER		16
#define MZ_BLUEHYPERBLASTER 17
#define MZ_PHALANX			18
#define MZ_SILENCED			128		// bit flag ORed with one of the above numbers

//ROGUE
#define MZ_ETF_RIFLE		30
#define MZ_UNUSED			31
#define MZ_SHOTGUN2			32
#define MZ_HEATBEAM			33
#define MZ_BLASTER2			34
#define	MZ_TRACKER			35
#define	MZ_NUKE1			36
#define	MZ_NUKE2			37
#define	MZ_NUKE4			38
#define	MZ_NUKE8			39
//ROGUE

//
// monster muzzle flashes
//
#define MZ2_TANK_BLASTER_1				1
#define MZ2_TANK_BLASTER_2				2
#define MZ2_TANK_BLASTER_3				3
#define MZ2_TANK_MACHINEGUN_1			4
#define MZ2_TANK_MACHINEGUN_2			5
#define MZ2_TANK_MACHINEGUN_3			6
#define MZ2_TANK_MACHINEGUN_4			7
#define MZ2_TANK_MACHINEGUN_5			8
#define MZ2_TANK_MACHINEGUN_6			9
#define MZ2_TANK_MACHINEGUN_7			10
#define MZ2_TANK_MACHINEGUN_8			11
#define MZ2_TANK_MACHINEGUN_9			12
#define MZ2_TANK_MACHINEGUN_10			13
#define MZ2_TANK_MACHINEGUN_11			14
#define MZ2_TANK_MACHINEGUN_12			15
#define MZ2_TANK_MACHINEGUN_13			16
#define MZ2_TANK_MACHINEGUN_14			17
#define MZ2_TANK_MACHINEGUN_15			18
#define MZ2_TANK_MACHINEGUN_16			19
#define MZ2_TANK_MACHINEGUN_17			20
#define MZ2_TANK_MACHINEGUN_18			21
#define MZ2_TANK_MACHINEGUN_19			22
#define MZ2_TANK_ROCKET_1				23
#define MZ2_TANK_ROCKET_2				24
#define MZ2_TANK_ROCKET_3				25

#define MZ2_INFANTRY_MACHINEGUN_1		26
#define MZ2_INFANTRY_MACHINEGUN_2		27
#define MZ2_INFANTRY_MACHINEGUN_3		28
#define MZ2_INFANTRY_MACHINEGUN_4		29
#define MZ2_INFANTRY_MACHINEGUN_5		30
#define MZ2_INFANTRY_MACHINEGUN_6		31
#define MZ2_INFANTRY_MACHINEGUN_7		32
#define MZ2_INFANTRY_MACHINEGUN_8		33
#define MZ2_INFANTRY_MACHINEGUN_9		34
#define MZ2_INFANTRY_MACHINEGUN_10		35
#define MZ2_INFANTRY_MACHINEGUN_11		36
#define MZ2_INFANTRY_MACHINEGUN_12		37
#define MZ2_INFANTRY_MACHINEGUN_13		38

#define MZ2_SOLDIER_BLASTER_1			39
#define MZ2_SOLDIER_BLASTER_2			40
#define MZ2_SOLDIER_SHOTGUN_1			41
#define MZ2_SOLDIER_SHOTGUN_2			42
#define MZ2_SOLDIER_MACHINEGUN_1		43
#define MZ2_SOLDIER_MACHINEGUN_2		44

#define MZ2_GUNNER_MACHINEGUN_1			45
#define MZ2_GUNNER_MACHINEGUN_2			46
#define MZ2_GUNNER_MACHINEGUN_3			47
#define MZ2_GUNNER_MACHINEGUN_4			48
#define MZ2_GUNNER_MACHINEGUN_5			49
#define MZ2_GUNNER_MACHINEGUN_6			50
#define MZ2_GUNNER_MACHINEGUN_7			51
#define MZ2_GUNNER_MACHINEGUN_8			52
#define MZ2_GUNNER_GRENADE_1			53
#define MZ2_GUNNER_GRENADE_2			54
#define MZ2_GUNNER_GRENADE_3			55
#define MZ2_GUNNER_GRENADE_4			56

#define MZ2_CHICK_ROCKET_1				57

#define MZ2_FLYER_BLASTER_1				58
#define MZ2_FLYER_BLASTER_2				59

#define MZ2_MEDIC_BLASTER_1				60

#define MZ2_GLADIATOR_RAILGUN_1			61

#define MZ2_HOVER_BLASTER_1				62

#define MZ2_ACTOR_MACHINEGUN_1			63

#define MZ2_SUPERTANK_MACHINEGUN_1		64
#define MZ2_SUPERTANK_MACHINEGUN_2		65
#define MZ2_SUPERTANK_MACHINEGUN_3		66
#define MZ2_SUPERTANK_MACHINEGUN_4		67
#define MZ2_SUPERTANK_MACHINEGUN_5		68
#define MZ2_SUPERTANK_MACHINEGUN_6		69
#define MZ2_SUPERTANK_ROCKET_1			70
#define MZ2_SUPERTANK_ROCKET_2			71
#define MZ2_SUPERTANK_ROCKET_3			72

#define MZ2_BOSS2_MACHINEGUN_L1			73
#define MZ2_BOSS2_MACHINEGUN_L2			74
#define MZ2_BOSS2_MACHINEGUN_L3			75
#define MZ2_BOSS2_MACHINEGUN_L4			76
#define MZ2_BOSS2_MACHINEGUN_L5			77
#define MZ2_BOSS2_ROCKET_1				78
#define MZ2_BOSS2_ROCKET_2				79
#define MZ2_BOSS2_ROCKET_3				80
#define MZ2_BOSS2_ROCKET_4				81

#define MZ2_FLOAT_BLASTER_1				82

#define MZ2_SOLDIER_BLASTER_3			83
#define MZ2_SOLDIER_SHOTGUN_3			84
#define MZ2_SOLDIER_MACHINEGUN_3		85
#define MZ2_SOLDIER_BLASTER_4			86
#define MZ2_SOLDIER_SHOTGUN_4			87
#define MZ2_SOLDIER_MACHINEGUN_4		88
#define MZ2_SOLDIER_BLASTER_5			89
#define MZ2_SOLDIER_SHOTGUN_5			90
#define MZ2_SOLDIER_MACHINEGUN_5		91
#define MZ2_SOLDIER_BLASTER_6			92
#define MZ2_SOLDIER_SHOTGUN_6			93
#define MZ2_SOLDIER_MACHINEGUN_6		94
#define MZ2_SOLDIER_BLASTER_7			95
#define MZ2_SOLDIER_SHOTGUN_7			96
#define MZ2_SOLDIER_MACHINEGUN_7		97
#define MZ2_SOLDIER_BLASTER_8			98
#define MZ2_SOLDIER_SHOTGUN_8			99
#define MZ2_SOLDIER_MACHINEGUN_8		100

// --- Xian shit below ---
#define	MZ2_MAKRON_BFG					101
#define MZ2_MAKRON_BLASTER_1			102
#define MZ2_MAKRON_BLASTER_2			103
#define MZ2_MAKRON_BLASTER_3			104
#define MZ2_MAKRON_BLASTER_4			105
#define MZ2_MAKRON_BLASTER_5			106
#define MZ2_MAKRON_BLASTER_6			107
#define MZ2_MAKRON_BLASTER_7			108
#define MZ2_MAKRON_BLASTER_8			109
#define MZ2_MAKRON_BLASTER_9			110
#define MZ2_MAKRON_BLASTER_10			111
#define MZ2_MAKRON_BLASTER_11			112
#define MZ2_MAKRON_BLASTER_12			113
#define MZ2_MAKRON_BLASTER_13			114
#define MZ2_MAKRON_BLASTER_14			115
#define MZ2_MAKRON_BLASTER_15			116
#define MZ2_MAKRON_BLASTER_16			117
#define MZ2_MAKRON_BLASTER_17			118
#define MZ2_MAKRON_RAILGUN_1			119
#define	MZ2_JORG_MACHINEGUN_L1			120
#define	MZ2_JORG_MACHINEGUN_L2			121
#define	MZ2_JORG_MACHINEGUN_L3			122
#define	MZ2_JORG_MACHINEGUN_L4			123
#define	MZ2_JORG_MACHINEGUN_L5			124
#define	MZ2_JORG_MACHINEGUN_L6			125
#define	MZ2_JORG_MACHINEGUN_R1			126
#define	MZ2_JORG_MACHINEGUN_R2			127
#define	MZ2_JORG_MACHINEGUN_R3			128
#define	MZ2_JORG_MACHINEGUN_R4			129
#define MZ2_JORG_MACHINEGUN_R5			130
#define	MZ2_JORG_MACHINEGUN_R6			131
#define MZ2_JORG_BFG_1					132
#define MZ2_BOSS2_MACHINEGUN_R1			133
#define MZ2_BOSS2_MACHINEGUN_R2			134
#define MZ2_BOSS2_MACHINEGUN_R3			135
#define MZ2_BOSS2_MACHINEGUN_R4			136
#define MZ2_BOSS2_MACHINEGUN_R5			137

//ROGUE
#define	MZ2_CARRIER_MACHINEGUN_L1		138
#define	MZ2_CARRIER_MACHINEGUN_R1		139
#define	MZ2_CARRIER_GRENADE				140
#define MZ2_TURRET_MACHINEGUN			141
#define MZ2_TURRET_ROCKET				142
#define MZ2_TURRET_BLASTER				143
#define MZ2_STALKER_BLASTER				144
#define MZ2_DAEDALUS_BLASTER			145
#define MZ2_MEDIC_BLASTER_2				146
#define	MZ2_CARRIER_RAILGUN				147
#define	MZ2_WIDOW_DISRUPTOR				148
#define	MZ2_WIDOW_BLASTER				149
#define	MZ2_WIDOW_RAIL					150
#define	MZ2_WIDOW_PLASMABEAM			151		// PMM - not used
#define	MZ2_CARRIER_MACHINEGUN_L2		152
#define	MZ2_CARRIER_MACHINEGUN_R2		153
#define	MZ2_WIDOW_RAIL_LEFT				154
#define	MZ2_WIDOW_RAIL_RIGHT			155
#define	MZ2_WIDOW_BLASTER_SWEEP1		156
#define	MZ2_WIDOW_BLASTER_SWEEP2		157
#define	MZ2_WIDOW_BLASTER_SWEEP3		158
#define	MZ2_WIDOW_BLASTER_SWEEP4		159
#define	MZ2_WIDOW_BLASTER_SWEEP5		160
#define	MZ2_WIDOW_BLASTER_SWEEP6		161
#define	MZ2_WIDOW_BLASTER_SWEEP7		162
#define	MZ2_WIDOW_BLASTER_SWEEP8		163
#define	MZ2_WIDOW_BLASTER_SWEEP9		164
#define	MZ2_WIDOW_BLASTER_100			165
#define	MZ2_WIDOW_BLASTER_90			166
#define	MZ2_WIDOW_BLASTER_80			167
#define	MZ2_WIDOW_BLASTER_70			168
#define	MZ2_WIDOW_BLASTER_60			169
#define	MZ2_WIDOW_BLASTER_50			170
#define	MZ2_WIDOW_BLASTER_40			171
#define	MZ2_WIDOW_BLASTER_30			172
#define	MZ2_WIDOW_BLASTER_20			173
#define	MZ2_WIDOW_BLASTER_10			174
#define	MZ2_WIDOW_BLASTER_0				175
#define	MZ2_WIDOW_BLASTER_10L			176
#define	MZ2_WIDOW_BLASTER_20L			177
#define	MZ2_WIDOW_BLASTER_30L			178
#define	MZ2_WIDOW_BLASTER_40L			179
#define	MZ2_WIDOW_BLASTER_50L			180
#define	MZ2_WIDOW_BLASTER_60L			181
#define	MZ2_WIDOW_BLASTER_70L			182
#define	MZ2_WIDOW_RUN_1					183
#define	MZ2_WIDOW_RUN_2					184
#define	MZ2_WIDOW_RUN_3					185
#define	MZ2_WIDOW_RUN_4					186
#define	MZ2_WIDOW_RUN_5					187
#define	MZ2_WIDOW_RUN_6					188
#define	MZ2_WIDOW_RUN_7					189
#define	MZ2_WIDOW_RUN_8					190
#define	MZ2_CARRIER_ROCKET_1			191
#define	MZ2_CARRIER_ROCKET_2			192
#define	MZ2_CARRIER_ROCKET_3			193
#define	MZ2_CARRIER_ROCKET_4			194
#define	MZ2_WIDOW2_BEAMER_1				195
#define	MZ2_WIDOW2_BEAMER_2				196
#define	MZ2_WIDOW2_BEAMER_3				197
#define	MZ2_WIDOW2_BEAMER_4				198
#define	MZ2_WIDOW2_BEAMER_5				199
#define	MZ2_WIDOW2_BEAM_SWEEP_1			200
#define	MZ2_WIDOW2_BEAM_SWEEP_2			201
#define	MZ2_WIDOW2_BEAM_SWEEP_3			202
#define	MZ2_WIDOW2_BEAM_SWEEP_4			203
#define	MZ2_WIDOW2_BEAM_SWEEP_5			204
#define	MZ2_WIDOW2_BEAM_SWEEP_6			205
#define	MZ2_WIDOW2_BEAM_SWEEP_7			206
#define	MZ2_WIDOW2_BEAM_SWEEP_8			207
#define	MZ2_WIDOW2_BEAM_SWEEP_9			208
#define	MZ2_WIDOW2_BEAM_SWEEP_10		209
#define	MZ2_WIDOW2_BEAM_SWEEP_11		210

// ROGUE

extern	const vec3_t monster_flash_offset [];


// temp entity events
//
// Temp entity events are for things that happen
// at a location seperate from any existing entity.
// Temporary entity messages are explicitly constructed
// and broadcast.
typedef enum
{
	TE_GUNSHOT,
	TE_BLOOD,
	TE_BLASTER,
	TE_RAILTRAIL,
	TE_SHOTGUN,
	TE_EXPLOSION1,
	TE_EXPLOSION2,
	TE_ROCKET_EXPLOSION,
	TE_GRENADE_EXPLOSION,
	TE_SPARKS,
	TE_SPLASH,
	TE_BUBBLETRAIL,
	TE_SCREEN_SPARKS,
	TE_SHIELD_SPARKS,
	TE_BULLET_SPARKS,
	TE_LASER_SPARKS,
	TE_PARASITE_ATTACK,
	TE_ROCKET_EXPLOSION_WATER,
	TE_GRENADE_EXPLOSION_WATER,
	TE_MEDIC_CABLE_ATTACK,
	TE_BFG_EXPLOSION,
	TE_BFG_BIGEXPLOSION,
	TE_BOSSTPORT,			// used as '22' in a map, so DON'T RENUMBER!!!
	TE_BFG_LASER,
	TE_GRAPPLE_CABLE,
	TE_WELDING_SPARKS,
	TE_GREENBLOOD,
	TE_BLUEHYPERBLASTER,
	TE_PLASMA_EXPLOSION,
	TE_TUNNEL_SPARKS,
//ROGUE
	TE_BLASTER2,
	TE_RAILTRAIL2,
	TE_FLAME,
	TE_LIGHTNING,
	TE_DEBUGTRAIL,
	TE_PLAIN_EXPLOSION,
	TE_FLASHLIGHT,
	TE_FORCEWALL,
	TE_HEATBEAM,
	TE_MONSTER_HEATBEAM,
	TE_STEAM,
	TE_BUBBLETRAIL2,
	TE_MOREBLOOD,
	TE_HEATBEAM_SPARKS,
	TE_HEATBEAM_STEAM,
	TE_CHAINFIST_SMOKE,
	TE_ELECTRIC_SPARKS,
	TE_TRACKER_EXPLOSION,
	TE_TELEPORT_EFFECT,
	TE_DBALL_GOAL,
	TE_WIDOWBEAMOUT,
	TE_NUKEBLAST,
	TE_WIDOWSPLASH,
	TE_EXPLOSION1_BIG,
	TE_EXPLOSION1_NP,
	TE_FLECHETTE
//ROGUE
} temp_event_t;

#define SPLASH_UNKNOWN		0
#define SPLASH_SPARKS		1
#define SPLASH_BLUE_WATER	2
#define SPLASH_BROWN_WATER	3
#define SPLASH_SLIME		4
#define	SPLASH_LAVA			5
#define SPLASH_BLOOD		6


// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define	CHAN_AUTO               0
#define	CHAN_WEAPON             1
#define	CHAN_VOICE              2
#define	CHAN_ITEM               3
#define	CHAN_BODY               4

// modifier flags
#define	CHAN_NO_PHS_ADD			8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define	CHAN_RELIABLE			16	// send by reliable message, not datagram
//R1Q2 SPECIFIC XXX
#define	CHAN_SERVER_ATTN_CALC	32

// sound attenuation values
#define	ATTN_NONE               0	// full volume the entire level
#define	ATTN_NORM               1
#define	ATTN_IDLE               2
#define	ATTN_STATIC             3	// diminish very rapidly with distance


// player_state->stats[] indexes
#define STAT_HEALTH_ICON		0
#define	STAT_HEALTH				1
#define	STAT_AMMO_ICON			2
#define	STAT_AMMO				3
#define	STAT_ARMOR_ICON			4
#define	STAT_ARMOR				5
#define	STAT_SELECTED_ICON		6
#define	STAT_PICKUP_ICON		7
#define	STAT_PICKUP_STRING		8
#define	STAT_TIMER_ICON			9
#define	STAT_TIMER				10
#define	STAT_HELPICON			11
#define	STAT_SELECTED_ITEM		12
#define	STAT_LAYOUTS			13
#define	STAT_FRAGS				14
#define	STAT_FLASHES			15		// cleared each frame, 1 = health, 2 = armor
#define STAT_CHASE				16
#define STAT_SPECTATOR			17

#define	MAX_STATS				32


// dmflags->value flags
#define	DF_NO_HEALTH		0x00000001	// 1
#define	DF_NO_ITEMS			0x00000002	// 2
#define	DF_WEAPONS_STAY		0x00000004	// 4
#define	DF_NO_FALLING		0x00000008	// 8
#define	DF_INSTANT_ITEMS	0x00000010	// 16
#define	DF_SAME_LEVEL		0x00000020	// 32
#define DF_SKINTEAMS		0x00000040	// 64
#define DF_MODELTEAMS		0x00000080	// 128
#define DF_NO_FRIENDLY_FIRE	0x00000100	// 256
#define	DF_SPAWN_FARTHEST	0x00000200	// 512
#define DF_FORCE_RESPAWN	0x00000400	// 1024
#define DF_NO_ARMOR			0x00000800	// 2048
#define DF_ALLOW_EXIT		0x00001000	// 4096
#define DF_INFINITE_AMMO	0x00002000	// 8192
#define DF_QUAD_DROP		0x00004000	// 16384
#define DF_FIXED_FOV		0x00008000	// 32768

// RAFAEL
#define	DF_QUADFIRE_DROP	0x00010000	// 65536

//ROGUE
#define DF_NO_MINES			0x00020000
#define DF_NO_STACK_DOUBLE	0x00040000
#define DF_NO_NUKES			0x00080000
#define DF_NO_SPHERES		0x00100000
//ROGUE

/*
ROGUE - VERSIONS
1234	08/13/1998		Activision
1235	08/14/1998		Id Software
1236	08/15/1998		Steve Tietze
1237	08/15/1998		Phil Dobranski
1238	08/15/1998		John Sheley
1239	08/17/1998		Barrett Alexander
1230	08/17/1998		Brandon Fish
1245	08/17/1998		Don MacAskill
1246	08/17/1998		David "Zoid" Kirsch
1247	08/17/1998		Manu Smith
1248	08/17/1998		Geoff Scully
1249	08/17/1998		Andy Van Fossen
1240	08/20/1998		Activision Build 2
1256	08/20/1998		Ranger Clan
1257	08/20/1998		Ensemble Studios
1258	08/21/1998		Robert Duffy
1259	08/21/1998		Stephen Seachord
1250	08/21/1998		Stephen Heaslip
1267	08/21/1998		Samir Sandesara
1268	08/21/1998		Oliver Wyman
1269	08/21/1998		Steven Marchegiano
1260	08/21/1998		Build #2 for Nihilistic
1278	08/21/1998		Build #2 for Ensemble

9999	08/20/1998		Internal Use
*/
#define ROGUE_VERSION_ID		1278

#define ROGUE_VERSION_STRING	"08/21/1998 Beta 2 for Ensemble"

// ROGUE
/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

#define	ANGLE2SHORT(x)	((int)((x)*182.04444444444444444444444444444f) & 65535)
#define	SHORT2ANGLE(x)	((x)*(0.0054931640625f))


//
// config strings are a general means of communication from
// the server to all connected clients.
// Each config string can be at most MAX_QPATH characters.
//
#define	CS_NAME				0
#define	CS_CDTRACK			1
#define	CS_SKY				2
#define	CS_SKYAXIS			3		// %f %f %f format
#define	CS_SKYROTATE		4
#define	CS_STATUSBAR		5		// display program string

#define CS_AIRACCEL			29		// air acceleration control
#define	CS_MAXCLIENTS		30
#define	CS_MAPCHECKSUM		31		// for catching cheater maps

#define	CS_MODELS			32
#define	CS_SOUNDS			(CS_MODELS+MAX_MODELS)			//288
#define	CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)			//544
#define	CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)			//800
#define	CS_ITEMS			(CS_LIGHTS+MAX_LIGHTSTYLES)		//1056
#define	CS_PLAYERSKINS		(CS_ITEMS+MAX_ITEMS)			//1312
#define CS_GENERAL			(CS_PLAYERSKINS+MAX_CLIENTS)	//1568
#define	MAX_CONFIGSTRINGS	(CS_GENERAL+MAX_GENERAL)		//2080


//==============================================


// entity_state_t->event values
// ertity events are for effects that take place reletive
// to an existing entities origin.  Very network efficient.
// All muzzle flashes really should be converted to events...
typedef enum
{
	EVENT_NONE,
	EV_ITEM_RESPAWN,
	EV_FOOTSTEP,
	EV_FALLSHORT,
	EV_FALL,
	EV_FALLFAR,
	EV_PLAYER_TELEPORT,
	EV_OTHER_TELEPORT
} entity_event_t;


// entity_state_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
typedef struct entity_state_s
{
	int		number;			// edict index

	vec3_t	origin;
	vec3_t	angles;
	vec3_t	old_origin;		// for lerping
	int32		modelindex;
	int32		modelindex2, modelindex3, modelindex4;	// weapons, CTF flags, etc
	int32		frame;
	int32		skinnum;
	uint32	effects;		// PGM - we're filling it, so it needs to be unsigned
	int32		renderfx;
	int32		solid;		// for client side prediction, 8*(bits 0-4) is x/y radius
							// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
							// gi.linkentity sets this properly
	int32		sound;		// for looping sounds, to guarantee shutoff
	int32		event;		// impulse events -- muzzle flashes, footsteps, etc
							// events only go out for a single frame, they
							// are automatically cleared each frame
} entity_state_t;

//==============================================


// player_state_t is the information needed in addition to pmove_state_t
// to rendered a view.  There will only be 10 player_state_t sent each second,
// but the number of pmove_state_t changes will be reletive to client
// frame rates

typedef struct
{
	pmove_state_t	pmove;		// for prediction

	// these fields do not need to be communicated bit-precise

	vec3_t		viewangles;		// for fixed views
	vec3_t		viewoffset;		// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
								// set by weapon kicks, pain effects, etc

	vec3_t		gunangles;
	vec3_t		gunoffset;

	int			gunindex;
	int			gunframe;

	float		blend[4];		// rgba full screen effect
	
	float		fov;			// horizontal field of view

	int			rdflags;		// refdef flags

	int16		stats[MAX_STATS];		// fast status bar updates
} player_state_t;

// ==================
// PGM 
#define VIDREF_GL		1
#define VIDREF_SOFT		2
#define VIDREF_OTHER	3

extern int vidref_val;
// PGM
// ==================

#define _QSHARED_H

#endif
