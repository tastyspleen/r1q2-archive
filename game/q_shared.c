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
#include "q_shared.h"

#define DEG2RAD( a ) (( a * M_PI ) / 180.0F)

vec3_t vec3_origin = {0,0,0};

//============================================================================

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];

	vec3_t vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	//r1: copies a bunch of stuff we overwrite, better to do individually
	//memcpy( im, m, sizeof( im ) );

	im[0][0] = m[0][0]; //r1
	im[0][1] = m[1][0];
	im[0][2] = m[2][0];

	im[1][0] = m[0][1];
	im[1][1] = m[1][1]; //r1
	im[1][2] = m[2][1];

	im[2][0] = m[0][2];
	im[2][1] = m[1][2];
	im[2][2] = m[2][2]; //r1

	//wtf?
	//memset( zrot, 0, sizeof( zrot ) );
	//zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = (float)cos( DEG2RAD( degrees ) );
	zrot[0][1] = (float)sin( DEG2RAD( degrees ) );
	zrot[0][2] = 0;

	zrot[1][0] = (float)-sin( DEG2RAD( degrees ) );
	zrot[1][1] = (float)cos( DEG2RAD( degrees ) );
	zrot[1][2] = 0;

	zrot[2][0] = 0.0f;
	zrot[2][1] = 0.0f;
	zrot[2][2] = 1.0f;

	R_ConcatRotations( m, zrot, tmpmat );
	R_ConcatRotations( tmpmat, im, rot );

	dst[0] = rot[0][0] * point[0] + rot[0][1] * point[1] + rot[0][2] * point[2];
	dst[1] = rot[1][0] * point[0] + rot[1][1] * point[1] + rot[1][2] * point[2];
	dst[2] = rot[2][0] * point[0] + rot[2][1] * point[1] + rot[2][2] * point[2];
}

void _Q_assert (char *expression, char *function, uint32 line)
{
	Com_Printf ("Q_assert: Assertion '%s' failed on %s:%u\n", LOG_GENERAL, expression, function, line);
#ifndef GAME_DLL
	Sys_DebugBreak ();
#endif
}

void AngleVectors (vec3_t angles, vec3_t /*@out@*//*@null@*/ forward, vec3_t /*@out@*//*@null@*/right, vec3_t /*@out@*//*@null@*/up)
{
	float		angle;
	static float		sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * M_PI2_DIV_360;
	sy = (float)sin(angle);
	cy = (float)cos(angle);

	angle = angles[PITCH] * M_PI2_DIV_360;
	sp = (float)sin(angle);
	cp = (float)cos(angle);

	if (right || up)
	{
		angle = angles[ROLL] * M_PI2_DIV_360;
		sr = (float)sin(angle);
		cr = (float)cos(angle);
	}

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}

	if (right)
	{
		right[0] = (-1*sr*sp*cy+-1*cr*-sy);
		right[1] = (-1*sr*sp*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}

	if (up)
	{
		up[0] = (cr*sp*cy+-sr*-sy);
		up[1] = (cr*sp*sy+-sr*cy);
		up[2] = cr*cp;
	}
}


void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec = {0.0, 0.0, 0.0};

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( (float)fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = (float)(float)fabs( src[i] );
		}
	}
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}



/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}


//============================================================================

//int sse2_enabled = 0;

#if defined _M_IX86 && !defined C_ONLY && !defined linux && !defined SSE2


/*__declspec( naked ) void __cdecl Q_ftol2( float f, int *out )
{
	__asm fld dword ptr [esp+4]
	__asm fistp dword ptr [esp+8]
	__asm ret
}*/

__declspec( naked ) int EXPORT Q_ftol( float f )
{
	__asm
	{
		fld dword ptr [esp+4]
		push eax
		fistp dword ptr [esp]
		pop eax
		ret
	}
}

/*__declspec( naked ) void __cdecl Q_ftolsse( float f, int *out )
{
	__asm movd xmm0, [esp+4]
	__asm cvttss2si eax, xmm0
	__asm mov [esp+8], eax
	__asm ret
}

__declspec( naked ) void __cdecl Q_ftol( float f, int *out )
{
	__asm cmp sse2_enabled, 0
	__asm jz nonsse
	__asm call Q_ftolsse
	__asm ret
nonsse:
	__asm call Q_ftol86
	__asm ret
}

__declspec (naked) void __cdecl Q_sseinit (void)
{
	__asm mov eax, 1
	__asm cpuid
	__asm test edx, 4000000h
	__asm jz nosse
	__asm mov sse2_enabled, 1
nosse:
	__asm ret
}*/

__declspec (naked) void EXPORT Q_fastfloats (float *f, int *outptr)
{
	/*__asm cmp sse2_enabled, 0
	__asm jz nonsse
	__asm mov eax, [esp+4]
	__asm movups xmm1, [eax]
	__asm cvttps2dq xmm0, xmm1
	__asm mov eax, [esp+8]
	__asm movdqu [eax], xmm0
	__asm ret
nonsse:*/
	__asm mov eax, [esp+8]
	__asm mov ebx, [esp+4]

	__asm fld dword ptr [ebx]
	__asm fistp dword ptr [eax]
	__asm fld dword ptr [ebx+4]
	__asm fistp dword ptr [eax+4]
	__asm fld dword ptr [ebx+8]
	__asm fistp dword ptr [eax+8]
	__asm ret
}

#else

int EXPORT Q_ftol( float f )
{
	return (int)f;
}

void EXPORT Q_fastfloats (float *f, int *outptr)
{
	outptr[0] = (int)f[0];
	outptr[1] = (int)f[1];
	outptr[2] = (int)f[2];
}

/*void Q_ftol2 (float f, int *out)
{
	*out = (int)f;
}*/

#endif

/*
===============
LerpAngle

===============
*/
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}


float	anglemod(float a)
{
#if 0
	if (a >= 0)
		a -= 360*(int)(a/360);
	else
		a += 360*( 1 + (int)(-a/360) );
#else
	//a = (0.0054931640625f) * ((int)(a*(182.0444444444444444444444444444444444444444444444444444444444444444f))& 65535);
	a = (360.0f/65536.0f) * ((int)(a*(65536.0f/360.0f)) & 65535);
#endif
	return a;
}

//int		i;
//vec3_t	corners[2];


// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (FLOAT_LT_ZERO(p->normal[i]))
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (FLOAT_GE_ZERO(dist1))
		sides = 1;
	if (FLOAT_LT_ZERO(dist2))
		sides |= 2;

	return sides;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
#if !id386 || defined __linux__ || defined __FreeBSD__
int EXPORT BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float	dist1, dist2;
	int		sides;

// fast axial cases
	//r1: these never seem to hit?
	/*if (p->type < 3)
	{
		Sys_DebugBreak ();
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}*/
	
// general case
	switch (p->signbits)
	{
	case 0:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	return sides;
}
#else
//#pragma warning( disable: 4035 )

__declspec( naked ) int __cdecl BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	//static int bops_initialized;
	static int Ljmptab[8];

	__asm
	{
		push esi
		push ebx
		
		lea eax, Ljmptab

		cmp dword ptr [eax], 0
		je short notinitialized
		//mov bops_initialized, 1
			
initialized:

		mov ecx,ds:dword ptr[8+4+esp]
		mov ebx,ds:dword ptr[8+8+esp]
		mov edx,ds:dword ptr[8+12+esp]
		;xor eax,eax
		;mov al,ds:byte ptr[17+edx]
		movzx esi, ds:byte ptr[17+edx]
		;r1 - removed, Lerror will crash and so will illegal access, this should
		;never happen anyway and just causes a branch
		;cmp al,8
		;jge Lerror
		fld ds:dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[eax+esi*4]
Lcase0:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase1:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase2:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase3:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase4:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase5:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase6:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp short LSetSides
Lcase7:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)

		;padding for LSetsides
		lea esp, [esp]
		lea esp, [esp]
LSetSides:
		faddp st(2),st(0)
		fcomp ds:dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp ds:dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		pop esi
		mov eax,ecx
		ret
//Lerror:
//		int 3
notinitialized:
		mov dword ptr [eax], offset Lcase0
		mov dword ptr [eax+4], offset Lcase1
		mov dword ptr [eax+8], offset Lcase2
		mov dword ptr [eax+12], offset Lcase3
		mov dword ptr [eax+16], offset Lcase4
		mov dword ptr [eax+20], offset Lcase5
		mov dword ptr [eax+24], offset Lcase6
		mov dword ptr [eax+28], offset Lcase7
		jmp initialized
	}
}
//#pragma warning( default: 4035 )
#endif

/*void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}*/

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	if (v[0] < mins[0])
		mins[0] = v[0];

	if (v[0] > maxs[0])
		maxs[0] = v[0];

	if (v[1] < mins[1])
		mins[1] = v[1];

	if (v[1] > maxs[1])
		maxs[1] = v[1];

	if (v[2] < mins[2])
		mins[2] = v[2];

	if (v[2] > maxs[2])
		maxs[2] = v[2];
}


//r1: macroified
/*int VectorCompare (vec3_t v1, vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
			return 0;
			
	return 1;
}*/


vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = (float)sqrt (length);		// FIXME

	if (FLOAT_NE_ZERO(length))
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

vec_t VectorNormalize2 (vec3_t v, vec3_t out)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = (float)sqrt (length);		// FIXME

	if (FLOAT_NE_ZERO(length))
	{
		ilength = 1/length;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	}
		
	return length;
}

/*void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}*/


vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void _VectorCopy (vec3_t in, vec3_t out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/*void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}*/

double EXPORT sqrt(double x);

vec_t VectorLength(vec3_t v)
{
	float	length;
	
	length = 0;

	length += v[0]*v[0];
	length += v[1]*v[1];
	length += v[2]*v[2];

	length = (float)sqrtf (length);		// FIXME

	return length;
}

/*void VectorInverse (vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (vec3_t in, vec_t scale, vec3_t out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}*/


int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}



//====================================================================================

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (pathname[0])
	{
		if (pathname[0] == '/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (const char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s, *s2;
	
	s = in + strlen(in) - 1;
	
	while (s != in && *s != '.')
		s--;
	
	for (s2 = s ; s2 != in && *s2 != '/' ; s2--)
	;
	
	if (s-s2 < 2)
		out[0] = 0;
	else
	{
		s--;
		strncpy (out,s2+1, s-s2);
		out[s-s2] = 0;
	}
}

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (const char *in, char *out)
{
	const char *s;
	
	s = in + strlen(in) - 1;
	
	while (s != in && *s != '/')
		s--;

	strncpy (out, in, s-in);
	out[s-in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, const char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

//r1: endianness sucks. this is a waste of function calls for everything.
//define Q_BIGENDIAN if you're on a mac or some other big endian system.

//need this for network ports
int16 ShortSwap (int16 l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

int32 LongSwap (int32 l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}

#if Q_BIGENDIAN

qboolean	bigendien;

// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places

int16	(*_LittleShort) (int16 l);
int32		(*_LittleLong) (int32 l);
float	(*_LittleFloat) (float l);

int16	LittleShort(int16 l) {return _LittleShort(l);}
int32		LittleLong (int32 l) {return _LittleLong(l);}
float	LittleFloat (float l) {return _LittleFloat(l);}

int16	ShortNoSwap (int16 l)
{
	return l;
}

int32	LongNoSwap (int32 l)
{
	return l;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
	byte	swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner	
	if ( *(int16 *)swaptest == 1)
	{
		bigendien = false;
		_LittleShort = ShortNoSwap;
		_LittleLong = LongNoSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		_LittleShort = ShortSwap;
		_LittleLong = LongSwap;
		_LittleFloat = FloatSwap;
	}
}
#else
void Swap_Init (void){}
#endif

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char	*va(const char *format, ...)
{
	va_list		argptr;

	static char		string[2][1024];
	static int		index;

	index  ^= 1;
	
	va_start (argptr, format);
	vsnprintf (string[index], sizeof(string[index])-1, format, argptr);
	va_end (argptr);

	return string[index];
}


char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char *COM_Parse (char **data_p)
{
	int		c;
	int		len;
	char	*data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return "";
	}
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		for (;;)
		{
			c = *data++;
			if (c== '\"' || !c)
			{
				//bugfix from skuller
				goto finish;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

finish:

	if (len == MAX_TOKEN_CHARS)
	{
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
===============
Com_PageInMemory

===============
*/
int	paged_total;

void Com_PageInMemory (byte *buffer, int size)
{
	int		i;

	for (i=size-1 ; i>0 ; i-=4096)
		paged_total += buffer[i];
}



/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

// FIXME: replace all Q_stricmp with Q_stricmp
#if defined _WIN32 && defined _M_AMD64
int Q_strncasecmp (const char *s1, const char *s2, size_t n)
{
	int		c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}

int Q_stricmp (const char *s1, const char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}
#endif


int Com_sprintf (char /*@out@*/*dest, int size, const char *fmt, ...)
{
	int			len;
	va_list		argptr;
	char		bigbuffer[0x10000];

	va_start (argptr,fmt);
	len = Q_vsnprintf (bigbuffer, sizeof(bigbuffer), fmt, argptr);
	va_end (argptr);

	if (len == -1 || len == size)
	{
		Com_Printf ("Com_sprintf: overflow of size %d\n", LOG_GENERAL, size);
		len = size-1;
	}

	bigbuffer[size-1] = '\0';
	strcpy (dest, bigbuffer);

	return len;
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (const char *s, const char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex ^= 1;
	if (*s == '\\')
		s++;
	for (;;)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

qboolean Info_KeyExists (const char *s, const char *key)
{
	char	pkey[512];
	char	*o;
	
	if (*s == '\\')
		s++;

	for (;;)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return false;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		while (*s != '\\' && *s)
			s++;

		if (!strcmp (key, pkey) )
			return true;

		if (!*s)
			return false;
		s++;
	}
}

void Info_RemoveKey (char *s, const char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strchr (key, '\\'))
	{
		Com_Printf ("Info_RemoveKey: Tried to remove illegal key '%s'\n", LOG_WARNING|LOG_GENERAL, key);
		return;
	}

	for (;;)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			//r1: overlapping src+dst with strcpy = no
			//strcpy (start, s);	// remove this part
			size_t memlen;

			memlen = strlen(s);
			memmove (start, s, memlen);
			start[memlen] = 0;
			return;
		}

		if (!*s)
			return;
	}

}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate (const char *s)
{
	if (strchr (s, '"'))
		return false;

	if (strchr (s, ';'))
		return false;

	return true;
}

qboolean Info_CheckBytes (const char *s)
{
	while (s[0])
	{
		if (s[0] < 32 || s[0] >= 127)
			return false;
		s++;
	}

	return true;
}

void Info_SetValueForKey (char *s, const char *key, const char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int		c;

	if (strchr (key, '\\') || strchr (value, '\\') )
	{
		Com_Printf ("Can't use keys or values with a \\ (attempted to set key '%s')\n", LOG_GENERAL, key);
		return;
	}

	if (strchr (key, ';') || strchr (value, ';') )
	{
		Com_Printf ("Can't use keys or values with a semicolon (attempted to set key '%s')\n", LOG_GENERAL, key);
		return;
	}

	if (strchr (key, '"') || strchr (value, '"') )
	{
		Com_Printf ("Can't use keys or values with a \" (attempted to set key '%s')\n", LOG_GENERAL, key);
		return;
	}

	if (strlen(key) > MAX_INFO_KEY-1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Com_Printf ("Keys and values must be < 64 characters (attempted to set key '%s')\n", LOG_GENERAL, key);
		return;
	}

	Info_RemoveKey (s, key);

	if (!value || !value[0])
		return;

	Com_sprintf (newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > MAX_INFO_STRING)
	{
		Com_Printf ("Info string length exceeded while trying to set '%s'\n", LOG_GENERAL, newi);
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;		// strip high bits
		if (c >= 32 && c < 127)
			*s++ = c;
	}
	s[0] = 0;
}

//====================================================================

#ifndef _WIN32
int Q_vsnprintf (char *buff, size_t len, const char *fmt, va_list va)
{
	int ret;

	ret = vsnprintf (buff, len, fmt, va);
	if (ret > -1 && ret < len)
		return ret;

	return -1;
}

/*int Q_snprintf (char *buff, size_t len, const char *fmt, ...)
{
	int ret;

	va_list argptr;

	va_start (argptr, fmt);
	ret = Q_vsnprintf (buff, len, fmt, argptr);
	va_end (argptr);

	return ret;
}*/

void Q_strlwr (char *str)
{
	while (*str)
	{
		if (isupper(*str))
			*str = tolower(*str);
		str++;
	}
}
#endif

/*
  Copyright (C) 1996, 1997, 1998, 1999, 2000 Florian Schintke
 
  This is free software; you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2, or (at your option) any later 
  version. 
 
  This is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
  for more details. 
 
  You should have received a copy of the GNU General Public License with 
  the c2html, java2html, pas2html or perl2html source package as the 
  file COPYING. If not, write to the Free Software Foundation, Inc., 
  59 Temple Place - Suite 330, Boston, MA 
  02111-1307, USA. 
*/

/* Documenttitle and purpose:                                       */
/*   Implementation of the UN*X wildcards in C. So they are         */
/*   available in a portable way and can be used whereever          */
/*   needed.                                                        */
/*                                                                  */
/* Version:                                                         */
/*   1.2                                                            */
/*                                                                  */
/* Test cases:                                                      */
/*   Please look into the Shellscript testwildcards.main            */
/*                                                                  */
/* Author(s):                                                       */
/*   Florian Schintke (schintke@gmx.de)                             */
/*                                                                  */
/* Tester:                                                          */
/*   Florian Schintke (schintke@gmx.de)                             */
/*                                                                  */
/* Testing state:                                                   */
/*   tested with the atac tool                                      */
/*   Latest test state is documented in testwildcards.report        */
/*   The program that tested this functions is available as         */
/*   testwildcards.c                                                */
/*                                                                  */
/* Dates:                                                           */
/*   First editing: unknown, but before 04/02/1997                  */
/*   Last Change  : 04/07/2000                                      */
/*                                                                  */
/* Known Bugs:                                                      */
/*                                                                  */

int set (char **wildcard, char **test);
/* Scans a set of characters and returns 0 if the set mismatches at this */
/* position in the teststring and 1 if it is matching                    */
/* wildcard is set to the closing ] and test is unmodified if mismatched */
/* and otherwise the char pointer is pointing to the next character      */

int asterisk (char **wildcard, char **test);
/* scans an asterisk */

int wildcardfit (char *wildcard, char *test)
{
  int fit = 1;
  
  for (; ('\000' != *wildcard) && (1 == fit) && ('\000' != *test); wildcard++)
    {
      switch (*wildcard)
        {
        case '[':
	  wildcard++; /* leave out the opening square bracket */ 
          fit = set (&wildcard, &test);
	  /* we don't need to decrement the wildcard as in case */
	  /* of asterisk because the closing ] is still there */
          break;
        case '?':
          test++;
          break;
        case '*':
          fit = asterisk (&wildcard, &test);
	  /* the asterisk was skipped by asterisk() but the loop will */
	  /* increment by itself. So we have to decrement */
	  wildcard--;
          break;
        default:
          fit = (int) (*wildcard == *test);
          test++;
        }
    }
  while ((*wildcard == '*') && (1 == fit)) 
    /* here the teststring is empty otherwise you cannot */
    /* leave the previous loop */ 
    wildcard++;
  return (int) ((1 == fit) && ('\0' == *test) && ('\0' == *wildcard));
}

int set (char **wildcard, char **test)
{
  int fit = 0;
  int negation = 0;
  int at_beginning = 1;

  if ('!' == **wildcard)
    {
      negation = 1;
      (*wildcard)++;
    }
  while ((']' != **wildcard) || (1 == at_beginning))
    {
      if (0 == fit)
        {
          if (('-' == **wildcard) 
              && ((*(*wildcard - 1)) < (*(*wildcard + 1)))
              && (']' != *(*wildcard + 1))
	      && (0 == at_beginning))
            {
              if (((**test) >= (*(*wildcard - 1)))
                  && ((**test) <= (*(*wildcard + 1))))
                {
                  fit = 1;
                  (*wildcard)++;
                }
            }
          else if ((**wildcard) == (**test))
            {
              fit = 1;
            }
        }
      (*wildcard)++;
      at_beginning = 0;
    }
  if (1 == negation)
    /* change from zero to one and vice versa */
    fit = 1 - fit;
  if (1 == fit) 
    (*test)++;

  return (fit);
}

int asterisk (char **wildcard, char **test)
{
  /* Warning: uses multiple returns */
  int fit = 1;

  /* erase the leading asterisk */
  (*wildcard)++; 
  while (('\000' != (**test))
	 && (('?' == **wildcard) 
	     || ('*' == **wildcard)))
    {
      if ('?' == **wildcard) 
	(*test)++;
      (*wildcard)++;
    }
  /* Now it could be that test is empty and wildcard contains */
  /* aterisks. Then we delete them to get a proper state */
  while ('*' == (**wildcard))
    (*wildcard)++;

  if (('\0' == (**test)) && ('\0' != (**wildcard)))
    return (fit = 0);
  if (('\0' == (**test)) && ('\0' == (**wildcard)))
    return (fit = 1); 
  else
    {
      /* Neither test nor wildcard are empty!          */
      /* the first character of wildcard isn't in [*?] */
      if (0 == wildcardfit(*wildcard, (*test)))
	{
	  do 
	    {
	      (*test)++;
	      /* skip as much characters as possible in the teststring */
	      /* stop if a character match occurs */
	      while (((**wildcard) != (**test)) 
		     && ('['  != (**wildcard))
		     && ('\0' != (**test)))
		(*test)++;
	    }
	  while ((('\0' != **test))? 
		 (0 == wildcardfit (*wildcard, (*test))) 
		 : (0 != (fit = 0)));
	}
      if (('\0' == **test) && ('\0' == **wildcard))
	fit = 1;
      return (fit);
    }
}
