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
/*
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake2 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/
#include <float.h>
#include "../ref_gl/gl_local.h"


#include "glw_win.h"

int   ( WINAPI * qwglChoosePixelFormat )(HDC, CONST PIXELFORMATDESCRIPTOR *);
int   ( WINAPI * qwglDescribePixelFormat) (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
int   ( WINAPI * qwglGetPixelFormat)(HDC);
BOOL  ( WINAPI * qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
BOOL  ( WINAPI * qwglSwapBuffers)(HDC);

BOOL  ( WINAPI * qwglCopyContext)(HGLRC, HGLRC, UINT);
HGLRC ( WINAPI * qwglCreateContext)(HDC);
HGLRC ( WINAPI * qwglCreateLayerContext)(HDC, int);
BOOL  ( WINAPI * qwglDeleteContext)(HGLRC);
HGLRC ( WINAPI * qwglGetCurrentContext)(VOID);
HDC   ( WINAPI * qwglGetCurrentDC)(VOID);
PROC  ( WINAPI * qwglGetProcAddress)(LPCSTR);
BOOL  ( WINAPI * qwglMakeCurrent)(HDC, HGLRC);
BOOL  ( WINAPI * qwglShareLists)(HGLRC, HGLRC);
BOOL  ( WINAPI * qwglUseFontBitmaps)(HDC, DWORD, DWORD, DWORD);

BOOL  ( WINAPI * qwglUseFontOutlines)(HDC, DWORD, DWORD, DWORD, FLOAT,
                                           FLOAT, int, LPGLYPHMETRICSFLOAT);

BOOL ( WINAPI * qwglDescribeLayerPlane)(HDC, int, int, UINT,
                                            LPLAYERPLANEDESCRIPTOR);
int  ( WINAPI * qwglSetLayerPaletteEntries)(HDC, int, int, int,
                                                CONST COLORREF *);
int  ( WINAPI * qwglGetLayerPaletteEntries)(HDC, int, int, int,
                                                COLORREF *);
BOOL ( WINAPI * qwglRealizeLayerPalette)(HDC, int, BOOL);
BOOL ( WINAPI * qwglSwapLayerBuffers)(HDC, UINT);

void ( APIENTRY * qglAccum )(GLenum op, GLfloat value);
void ( APIENTRY * qglAlphaFunc )(GLenum func, GLclampf ref);
GLboolean ( APIENTRY * qglAreTexturesResident )(GLsizei n, const GLuint *textures, GLboolean *residences);
void ( APIENTRY * qglArrayElement )(GLint i);
void ( APIENTRY * qglBegin )(GLenum mode);
void ( APIENTRY * qglBindTexture )(GLenum target, GLuint texture);
void ( APIENTRY * qglBitmap )(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
void ( APIENTRY * qglBlendFunc )(GLenum sfactor, GLenum dfactor);
void ( APIENTRY * qglCallList )(GLuint list);
void ( APIENTRY * qglCallLists )(GLsizei n, GLenum type, const GLvoid *lists);
void ( APIENTRY * qglClear )(GLbitfield mask);
void ( APIENTRY * qglClearAccum )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( APIENTRY * qglClearColor )(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void ( APIENTRY * qglClearDepth )(GLclampd depth);
void ( APIENTRY * qglClearIndex )(GLfloat c);
void ( APIENTRY * qglClearStencil )(GLint s);
void ( APIENTRY * qglClipPlane )(GLenum plane, const GLdouble *equation);
void ( APIENTRY * qglColor3b )(GLbyte red, GLbyte green, GLbyte blue);
void ( APIENTRY * qglColor3bv )(const GLbyte *v);
void ( APIENTRY * qglColor3d )(GLdouble red, GLdouble green, GLdouble blue);
void ( APIENTRY * qglColor3dv )(const GLdouble *v);
void ( APIENTRY * qglColor3f )(GLfloat red, GLfloat green, GLfloat blue);
void ( APIENTRY * qglColor3fv )(const GLfloat *v);
void ( APIENTRY * qglColor3i )(GLint red, GLint green, GLint blue);
void ( APIENTRY * qglColor3iv )(const GLint *v);
void ( APIENTRY * qglColor3s )(GLshort red, GLshort green, GLshort blue);
void ( APIENTRY * qglColor3sv )(const GLshort *v);
void ( APIENTRY * qglColor3ub )(GLubyte red, GLubyte green, GLubyte blue);
void ( APIENTRY * qglColor3ubv )(const GLubyte *v);
void ( APIENTRY * qglColor3ui )(GLuint red, GLuint green, GLuint blue);
void ( APIENTRY * qglColor3uiv )(const GLuint *v);
void ( APIENTRY * qglColor3us )(GLushort red, GLushort green, GLushort blue);
void ( APIENTRY * qglColor3usv )(const GLushort *v);
void ( APIENTRY * qglColor4b )(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
void ( APIENTRY * qglColor4bv )(const GLbyte *v);
void ( APIENTRY * qglColor4d )(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
void ( APIENTRY * qglColor4dv )(const GLdouble *v);
void ( APIENTRY * qglColor4f )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( APIENTRY * qglColor4fv )(const GLfloat *v);
void ( APIENTRY * qglColor4i )(GLint red, GLint green, GLint blue, GLint alpha);
void ( APIENTRY * qglColor4iv )(const GLint *v);
void ( APIENTRY * qglColor4s )(GLshort red, GLshort green, GLshort blue, GLshort alpha);
void ( APIENTRY * qglColor4sv )(const GLshort *v);
void ( APIENTRY * qglColor4ub )(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void ( APIENTRY * qglColor4ubv )(const GLubyte *v);
void ( APIENTRY * qglColor4ui )(GLuint red, GLuint green, GLuint blue, GLuint alpha);
void ( APIENTRY * qglColor4uiv )(const GLuint *v);
void ( APIENTRY * qglColor4us )(GLushort red, GLushort green, GLushort blue, GLushort alpha);
void ( APIENTRY * qglColor4usv )(const GLushort *v);
void ( APIENTRY * qglColorMask )(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void ( APIENTRY * qglColorMaterial )(GLenum face, GLenum mode);
void ( APIENTRY * qglColorPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglCopyPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void ( APIENTRY * qglCopyTexImage1D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
void ( APIENTRY * qglCopyTexImage2D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void ( APIENTRY * qglCopyTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void ( APIENTRY * qglCopyTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglCullFace )(GLenum mode);
void ( APIENTRY * qglDeleteLists )(GLuint list, GLsizei range);
void ( APIENTRY * qglDeleteTextures )(GLsizei n, const GLuint *textures);
void ( APIENTRY * qglDepthFunc )(GLenum func);
void ( APIENTRY * qglDepthMask )(GLboolean flag);
void ( APIENTRY * qglDepthRange )(GLclampd zNear, GLclampd zFar);
void ( APIENTRY * qglDisable )(GLenum cap);
void ( APIENTRY * qglDisableClientState )(GLenum array);
void ( APIENTRY * qglDrawArrays )(GLenum mode, GLint first, GLsizei count);
void ( APIENTRY * qglDrawBuffer )(GLenum mode);
void ( APIENTRY * qglDrawElements )(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void ( APIENTRY * qglDrawPixels )(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglEdgeFlag )(GLboolean flag);
void ( APIENTRY * qglEdgeFlagPointer )(GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglEdgeFlagv )(const GLboolean *flag);
void ( APIENTRY * qglEnable )(GLenum cap);
void ( APIENTRY * qglEnableClientState )(GLenum array);
void ( APIENTRY * qglEnd )(void);
void ( APIENTRY * qglEndList )(void);
/*void ( APIENTRY * qglEvalCoord1d )(GLdouble u);
void ( APIENTRY * qglEvalCoord1dv )(const GLdouble *u);
void ( APIENTRY * qglEvalCoord1f )(GLfloat u);
void ( APIENTRY * qglEvalCoord1fv )(const GLfloat *u);
void ( APIENTRY * qglEvalCoord2d )(GLdouble u, GLdouble v);
void ( APIENTRY * qglEvalCoord2dv )(const GLdouble *u);
void ( APIENTRY * qglEvalCoord2f )(GLfloat u, GLfloat v);
void ( APIENTRY * qglEvalCoord2fv )(const GLfloat *u);
void ( APIENTRY * qglEvalMesh1 )(GLenum mode, GLint i1, GLint i2);
void ( APIENTRY * qglEvalMesh2 )(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void ( APIENTRY * qglEvalPoint1 )(GLint i);
void ( APIENTRY * qglEvalPoint2 )(GLint i, GLint j);*/
void ( APIENTRY * qglFeedbackBuffer )(GLsizei size, GLenum type, GLfloat *buffer);
void ( APIENTRY * qglFinish )(void);
void ( APIENTRY * qglFlush )(void);
void ( APIENTRY * qglFogf )(GLenum pname, GLfloat param);
void ( APIENTRY * qglFogfv )(GLenum pname, const GLfloat *params);
void ( APIENTRY * qglFogi )(GLenum pname, GLint param);
void ( APIENTRY * qglFogiv )(GLenum pname, const GLint *params);
void ( APIENTRY * qglFrontFace )(GLenum mode);
void ( APIENTRY * qglFrustum )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint ( APIENTRY * qglGenLists )(GLsizei range);
void ( APIENTRY * qglGenTextures )(GLsizei n, GLuint *textures);
void ( APIENTRY * qglGetBooleanv )(GLenum pname, GLboolean *params);
void ( APIENTRY * qglGetClipPlane )(GLenum plane, GLdouble *equation);
void ( APIENTRY * qglGetDoublev )(GLenum pname, GLdouble *params);
GLenum ( APIENTRY * qglGetError )(void);
void ( APIENTRY * qglGetFloatv )(GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetIntegerv )(GLenum pname, GLint *params);
void ( APIENTRY * qglGetLightfv )(GLenum light, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetLightiv )(GLenum light, GLenum pname, GLint *params);
void ( APIENTRY * qglGetMapdv )(GLenum target, GLenum query, GLdouble *v);
void ( APIENTRY * qglGetMapfv )(GLenum target, GLenum query, GLfloat *v);
void ( APIENTRY * qglGetMapiv )(GLenum target, GLenum query, GLint *v);
void ( APIENTRY * qglGetMaterialfv )(GLenum face, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetMaterialiv )(GLenum face, GLenum pname, GLint *params);
void ( APIENTRY * qglGetPixelMapfv )(GLenum map, GLfloat *values);
void ( APIENTRY * qglGetPixelMapuiv )(GLenum map, GLuint *values);
void ( APIENTRY * qglGetPixelMapusv )(GLenum map, GLushort *values);
void ( APIENTRY * qglGetPointerv )(GLenum pname, GLvoid* *params);
void ( APIENTRY * qglGetPolygonStipple )(GLubyte *mask);
const GLubyte * ( APIENTRY * qglGetString )(GLenum name);
void ( APIENTRY * qglGetTexEnvfv )(GLenum target, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexEnviv )(GLenum target, GLenum pname, GLint *params);
void ( APIENTRY * qglGetTexGendv )(GLenum coord, GLenum pname, GLdouble *params);
void ( APIENTRY * qglGetTexGenfv )(GLenum coord, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexGeniv )(GLenum coord, GLenum pname, GLint *params);
void ( APIENTRY * qglGetTexImage )(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
//void ( APIENTRY * qglGetTexLevelParameterfv )(GLenum target, GLint level, GLenum pname, GLfloat *params);
//void ( APIENTRY * qglGetTexLevelParameteriv )(GLenum target, GLint level, GLenum pname, GLint *params);
void ( APIENTRY * qglGetTexParameterfv )(GLenum target, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexParameteriv )(GLenum target, GLenum pname, GLint *params);
void ( APIENTRY * qglHint )(GLenum target, GLenum mode);
void ( APIENTRY * qglIndexMask )(GLuint mask);
void ( APIENTRY * qglIndexPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglIndexd )(GLdouble c);
void ( APIENTRY * qglIndexdv )(const GLdouble *c);
void ( APIENTRY * qglIndexf )(GLfloat c);
void ( APIENTRY * qglIndexfv )(const GLfloat *c);
void ( APIENTRY * qglIndexi )(GLint c);
void ( APIENTRY * qglIndexiv )(const GLint *c);
void ( APIENTRY * qglIndexs )(GLshort c);
void ( APIENTRY * qglIndexsv )(const GLshort *c);
void ( APIENTRY * qglIndexub )(GLubyte c);
void ( APIENTRY * qglIndexubv )(const GLubyte *c);
void ( APIENTRY * qglInitNames )(void);
void ( APIENTRY * qglInterleavedArrays )(GLenum format, GLsizei stride, const GLvoid *pointer);
GLboolean ( APIENTRY * qglIsEnabled )(GLenum cap);
GLboolean ( APIENTRY * qglIsList )(GLuint list);
GLboolean ( APIENTRY * qglIsTexture )(GLuint texture);
void ( APIENTRY * qglLightModelf )(GLenum pname, GLfloat param);
void ( APIENTRY * qglLightModelfv )(GLenum pname, const GLfloat *params);
void ( APIENTRY * qglLightModeli )(GLenum pname, GLint param);
void ( APIENTRY * qglLightModeliv )(GLenum pname, const GLint *params);
void ( APIENTRY * qglLightf )(GLenum light, GLenum pname, GLfloat param);
void ( APIENTRY * qglLightfv )(GLenum light, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglLighti )(GLenum light, GLenum pname, GLint param);
void ( APIENTRY * qglLightiv )(GLenum light, GLenum pname, const GLint *params);
void ( APIENTRY * qglLineStipple )(GLint factor, GLushort pattern);
void ( APIENTRY * qglLineWidth )(GLfloat width);
void ( APIENTRY * qglListBase )(GLuint base);
void ( APIENTRY * qglLoadIdentity )(void);
void ( APIENTRY * qglLoadMatrixd )(const GLdouble *m);
void ( APIENTRY * qglLoadMatrixf )(const GLfloat *m);
void ( APIENTRY * qglLoadName )(GLuint name);
void ( APIENTRY * qglLogicOp )(GLenum opcode);
/*void ( APIENTRY * qglMap1d )(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
void ( APIENTRY * qglMap1f )(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
void ( APIENTRY * qglMap2d )(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
void ( APIENTRY * qglMap2f )(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
void ( APIENTRY * qglMapGrid1d )(GLint un, GLdouble u1, GLdouble u2);
void ( APIENTRY * qglMapGrid1f )(GLint un, GLfloat u1, GLfloat u2);
void ( APIENTRY * qglMapGrid2d )(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
void ( APIENTRY * qglMapGrid2f )(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void ( APIENTRY * qglMaterialf )(GLenum face, GLenum pname, GLfloat param);
void ( APIENTRY * qglMaterialfv )(GLenum face, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglMateriali )(GLenum face, GLenum pname, GLint param);
void ( APIENTRY * qglMaterialiv )(GLenum face, GLenum pname, const GLint *params);*/
void ( APIENTRY * qglMatrixMode )(GLenum mode);
void ( APIENTRY * qglMultMatrixd )(const GLdouble *m);
void ( APIENTRY * qglMultMatrixf )(const GLfloat *m);
void ( APIENTRY * qglNewList )(GLuint list, GLenum mode);
void ( APIENTRY * qglNormal3b )(GLbyte nx, GLbyte ny, GLbyte nz);
void ( APIENTRY * qglNormal3bv )(const GLbyte *v);
void ( APIENTRY * qglNormal3d )(GLdouble nx, GLdouble ny, GLdouble nz);
void ( APIENTRY * qglNormal3dv )(const GLdouble *v);
void ( APIENTRY * qglNormal3f )(GLfloat nx, GLfloat ny, GLfloat nz);
void ( APIENTRY * qglNormal3fv )(const GLfloat *v);
void ( APIENTRY * qglNormal3i )(GLint nx, GLint ny, GLint nz);
void ( APIENTRY * qglNormal3iv )(const GLint *v);
void ( APIENTRY * qglNormal3s )(GLshort nx, GLshort ny, GLshort nz);
void ( APIENTRY * qglNormal3sv )(const GLshort *v);
void ( APIENTRY * qglNormalPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglOrtho )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void ( APIENTRY * qglPassThrough )(GLfloat token);
void ( APIENTRY * qglPixelMapfv )(GLenum map, GLsizei mapsize, const GLfloat *values);
void ( APIENTRY * qglPixelMapuiv )(GLenum map, GLsizei mapsize, const GLuint *values);
void ( APIENTRY * qglPixelMapusv )(GLenum map, GLsizei mapsize, const GLushort *values);
void ( APIENTRY * qglPixelStoref )(GLenum pname, GLfloat param);
void ( APIENTRY * qglPixelStorei )(GLenum pname, GLint param);
void ( APIENTRY * qglPixelTransferf )(GLenum pname, GLfloat param);
void ( APIENTRY * qglPixelTransferi )(GLenum pname, GLint param);
void ( APIENTRY * qglPixelZoom )(GLfloat xfactor, GLfloat yfactor);
void ( APIENTRY * qglPointSize )(GLfloat size);
void ( APIENTRY * qglPolygonMode )(GLenum face, GLenum mode);
void ( APIENTRY * qglPolygonOffset )(GLfloat factor, GLfloat units);
void ( APIENTRY * qglPolygonStipple )(const GLubyte *mask);
void ( APIENTRY * qglPopAttrib )(void);
void ( APIENTRY * qglPopClientAttrib )(void);
void ( APIENTRY * qglPopMatrix )(void);
void ( APIENTRY * qglPopName )(void);
void ( APIENTRY * qglPrioritizeTextures )(GLsizei n, const GLuint *textures, const GLclampf *priorities);
void ( APIENTRY * qglPushAttrib )(GLbitfield mask);
void ( APIENTRY * qglPushClientAttrib )(GLbitfield mask);
void ( APIENTRY * qglPushMatrix )(void);
void ( APIENTRY * qglPushName )(GLuint name);
/*void ( APIENTRY * qglRasterPos2d )(GLdouble x, GLdouble y);
void ( APIENTRY * qglRasterPos2dv )(const GLdouble *v);
void ( APIENTRY * qglRasterPos2f )(GLfloat x, GLfloat y);
void ( APIENTRY * qglRasterPos2fv )(const GLfloat *v);
void ( APIENTRY * qglRasterPos2i )(GLint x, GLint y);
void ( APIENTRY * qglRasterPos2iv )(const GLint *v);
void ( APIENTRY * qglRasterPos2s )(GLshort x, GLshort y);
void ( APIENTRY * qglRasterPos2sv )(const GLshort *v);
void ( APIENTRY * qglRasterPos3d )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglRasterPos3dv )(const GLdouble *v);
void ( APIENTRY * qglRasterPos3f )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglRasterPos3fv )(const GLfloat *v);
void ( APIENTRY * qglRasterPos3i )(GLint x, GLint y, GLint z);
void ( APIENTRY * qglRasterPos3iv )(const GLint *v);
void ( APIENTRY * qglRasterPos3s )(GLshort x, GLshort y, GLshort z);
void ( APIENTRY * qglRasterPos3sv )(const GLshort *v);
void ( APIENTRY * qglRasterPos4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void ( APIENTRY * qglRasterPos4dv )(const GLdouble *v);
void ( APIENTRY * qglRasterPos4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void ( APIENTRY * qglRasterPos4fv )(const GLfloat *v);
void ( APIENTRY * qglRasterPos4i )(GLint x, GLint y, GLint z, GLint w);
void ( APIENTRY * qglRasterPos4iv )(const GLint *v);
void ( APIENTRY * qglRasterPos4s )(GLshort x, GLshort y, GLshort z, GLshort w);
void ( APIENTRY * qglRasterPos4sv )(const GLshort *v);*/
void ( APIENTRY * qglReadBuffer )(GLenum mode);
void ( APIENTRY * qglReadPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void ( APIENTRY * qglRectd )(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
void ( APIENTRY * qglRectdv )(const GLdouble *v1, const GLdouble *v2);
void ( APIENTRY * qglRectf )(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void ( APIENTRY * qglRectfv )(const GLfloat *v1, const GLfloat *v2);
void ( APIENTRY * qglRecti )(GLint x1, GLint y1, GLint x2, GLint y2);
void ( APIENTRY * qglRectiv )(const GLint *v1, const GLint *v2);
void ( APIENTRY * qglRects )(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void ( APIENTRY * qglRectsv )(const GLshort *v1, const GLshort *v2);
GLint ( APIENTRY * qglRenderMode )(GLenum mode);
void ( APIENTRY * qglRotated )(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglRotatef )(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglScaled )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglScalef )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglScissor )(GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglSelectBuffer )(GLsizei size, GLuint *buffer);
void ( APIENTRY * qglShadeModel )(GLenum mode);
void ( APIENTRY * qglStencilFunc )(GLenum func, GLint ref, GLuint mask);
void ( APIENTRY * qglStencilMask )(GLuint mask);
void ( APIENTRY * qglStencilOp )(GLenum fail, GLenum zfail, GLenum zpass);
void ( APIENTRY * qglTexCoord1d )(GLdouble s);
void ( APIENTRY * qglTexCoord1dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord1f )(GLfloat s);
void ( APIENTRY * qglTexCoord1fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord1i )(GLint s);
void ( APIENTRY * qglTexCoord1iv )(const GLint *v);
void ( APIENTRY * qglTexCoord1s )(GLshort s);
void ( APIENTRY * qglTexCoord1sv )(const GLshort *v);
void ( APIENTRY * qglTexCoord2d )(GLdouble s, GLdouble t);
void ( APIENTRY * qglTexCoord2dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord2f )(GLfloat s, GLfloat t);
void ( APIENTRY * qglTexCoord2fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord2i )(GLint s, GLint t);
void ( APIENTRY * qglTexCoord2iv )(const GLint *v);
void ( APIENTRY * qglTexCoord2s )(GLshort s, GLshort t);
void ( APIENTRY * qglTexCoord2sv )(const GLshort *v);
void ( APIENTRY * qglTexCoord3d )(GLdouble s, GLdouble t, GLdouble r);
void ( APIENTRY * qglTexCoord3dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord3f )(GLfloat s, GLfloat t, GLfloat r);
void ( APIENTRY * qglTexCoord3fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord3i )(GLint s, GLint t, GLint r);
void ( APIENTRY * qglTexCoord3iv )(const GLint *v);
void ( APIENTRY * qglTexCoord3s )(GLshort s, GLshort t, GLshort r);
void ( APIENTRY * qglTexCoord3sv )(const GLshort *v);
void ( APIENTRY * qglTexCoord4d )(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void ( APIENTRY * qglTexCoord4dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord4f )(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void ( APIENTRY * qglTexCoord4fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord4i )(GLint s, GLint t, GLint r, GLint q);
void ( APIENTRY * qglTexCoord4iv )(const GLint *v);
void ( APIENTRY * qglTexCoord4s )(GLshort s, GLshort t, GLshort r, GLshort q);
void ( APIENTRY * qglTexCoord4sv )(const GLshort *v);
void ( APIENTRY * qglTexCoordPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglTexEnvf )(GLenum target, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexEnvfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexEnvi )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglTexEnviv )(GLenum target, GLenum pname, const GLint *params);
void ( APIENTRY * qglTexGend )(GLenum coord, GLenum pname, GLdouble param);
void ( APIENTRY * qglTexGendv )(GLenum coord, GLenum pname, const GLdouble *params);
void ( APIENTRY * qglTexGenf )(GLenum coord, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexGenfv )(GLenum coord, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexGeni )(GLenum coord, GLenum pname, GLint param);
void ( APIENTRY * qglTexGeniv )(GLenum coord, GLenum pname, const GLint *params);
void ( APIENTRY * qglTexImage1D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexParameterf )(GLenum target, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexParameterfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexParameteri )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglTexParameteriv )(GLenum target, GLenum pname, const GLint *params);
void ( APIENTRY * qglTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTranslated )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglTranslatef )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglVertex2d )(GLdouble x, GLdouble y);
void ( APIENTRY * qglVertex2dv )(const GLdouble *v);
void ( APIENTRY * qglVertex2f )(GLfloat x, GLfloat y);
void ( APIENTRY * qglVertex2fv )(const GLfloat *v);
void ( APIENTRY * qglVertex2i )(GLint x, GLint y);
void ( APIENTRY * qglVertex2iv )(const GLint *v);
void ( APIENTRY * qglVertex2s )(GLshort x, GLshort y);
void ( APIENTRY * qglVertex2sv )(const GLshort *v);
void ( APIENTRY * qglVertex3d )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglVertex3dv )(const GLdouble *v);
void ( APIENTRY * qglVertex3f )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglVertex3fv )(const GLfloat *v);
void ( APIENTRY * qglVertex3i )(GLint x, GLint y, GLint z);
void ( APIENTRY * qglVertex3iv )(const GLint *v);
void ( APIENTRY * qglVertex3s )(GLshort x, GLshort y, GLshort z);
void ( APIENTRY * qglVertex3sv )(const GLshort *v);
void ( APIENTRY * qglVertex4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void ( APIENTRY * qglVertex4dv )(const GLdouble *v);
void ( APIENTRY * qglVertex4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void ( APIENTRY * qglVertex4fv )(const GLfloat *v);
void ( APIENTRY * qglVertex4i )(GLint x, GLint y, GLint z, GLint w);
void ( APIENTRY * qglVertex4iv )(const GLint *v);
void ( APIENTRY * qglVertex4s )(GLshort x, GLshort y, GLshort z, GLshort w);
void ( APIENTRY * qglVertex4sv )(const GLshort *v);
void ( APIENTRY * qglVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglViewport )(GLint x, GLint y, GLsizei width, GLsizei height);

void ( APIENTRY * qglLockArraysEXT)( int, int);
void ( APIENTRY * qglUnlockArraysEXT) ( void );

BOOL ( WINAPI * qwglSwapIntervalEXT)( int interval );
BOOL ( WINAPI * qwglGetDeviceGammaRampEXT)( unsigned char *, unsigned char *, unsigned char * );
BOOL ( WINAPI * qwglSetDeviceGammaRampEXT)( const unsigned char *, const unsigned char *, const unsigned char * );
void ( APIENTRY * qglPointParameterfEXT)( GLenum param, GLfloat value );
void ( APIENTRY * qglPointParameterfvEXT)( GLenum param, const GLfloat *value );
void ( APIENTRY * qglColorTableEXT)( int, int, int, int, int, const void * );
void ( APIENTRY * qglSelectTextureSGIS)( GLenum );
void ( APIENTRY * qglMTexCoord2fSGIS)( GLenum, GLfloat, GLfloat );
void ( APIENTRY * qglMTexCoord2fvSGIS)( GLenum, GLfloat *);
void ( APIENTRY * qglActiveTextureARB) ( GLenum );
void ( APIENTRY * qglClientActiveTextureARB) ( GLenum );

//r1ch
void ( APIENTRY * qglPointParameterfARB) (GLenum, GLfloat);
void ( APIENTRY * qglPointParameterfvARB) (GLenum, const GLfloat *);

static void ( APIENTRY * dllAccum )(GLenum op, GLfloat value);
static void ( APIENTRY * dllAlphaFunc )(GLenum func, GLclampf ref);
GLboolean ( APIENTRY * dllAreTexturesResident )(GLsizei n, const GLuint *textures, GLboolean *residences);
static void ( APIENTRY * dllArrayElement )(GLint i);
static void ( APIENTRY * dllBegin )(GLenum mode);
static void ( APIENTRY * dllBindTexture )(GLenum target, GLuint texture);
static void ( APIENTRY * dllBitmap )(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
static void ( APIENTRY * dllBlendFunc )(GLenum sfactor, GLenum dfactor);
static void ( APIENTRY * dllCallList )(GLuint list);
static void ( APIENTRY * dllCallLists )(GLsizei n, GLenum type, const GLvoid *lists);
static void ( APIENTRY * dllClear )(GLbitfield mask);
static void ( APIENTRY * dllClearAccum )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
static void ( APIENTRY * dllClearColor )(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
static void ( APIENTRY * dllClearDepth )(GLclampd depth);
static void ( APIENTRY * dllClearIndex )(GLfloat c);
static void ( APIENTRY * dllClearStencil )(GLint s);
static void ( APIENTRY * dllClipPlane )(GLenum plane, const GLdouble *equation);
static void ( APIENTRY * dllColor3b )(GLbyte red, GLbyte green, GLbyte blue);
static void ( APIENTRY * dllColor3bv )(const GLbyte *v);
static void ( APIENTRY * dllColor3d )(GLdouble red, GLdouble green, GLdouble blue);
static void ( APIENTRY * dllColor3dv )(const GLdouble *v);
static void ( APIENTRY * dllColor3f )(GLfloat red, GLfloat green, GLfloat blue);
static void ( APIENTRY * dllColor3fv )(const GLfloat *v);
static void ( APIENTRY * dllColor3i )(GLint red, GLint green, GLint blue);
static void ( APIENTRY * dllColor3iv )(const GLint *v);
static void ( APIENTRY * dllColor3s )(GLshort red, GLshort green, GLshort blue);
static void ( APIENTRY * dllColor3sv )(const GLshort *v);
static void ( APIENTRY * dllColor3ub )(GLubyte red, GLubyte green, GLubyte blue);
static void ( APIENTRY * dllColor3ubv )(const GLubyte *v);
static void ( APIENTRY * dllColor3ui )(GLuint red, GLuint green, GLuint blue);
static void ( APIENTRY * dllColor3uiv )(const GLuint *v);
static void ( APIENTRY * dllColor3us )(GLushort red, GLushort green, GLushort blue);
static void ( APIENTRY * dllColor3usv )(const GLushort *v);
static void ( APIENTRY * dllColor4b )(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
static void ( APIENTRY * dllColor4bv )(const GLbyte *v);
static void ( APIENTRY * dllColor4d )(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
static void ( APIENTRY * dllColor4dv )(const GLdouble *v);
static void ( APIENTRY * dllColor4f )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
static void ( APIENTRY * dllColor4fv )(const GLfloat *v);
static void ( APIENTRY * dllColor4i )(GLint red, GLint green, GLint blue, GLint alpha);
static void ( APIENTRY * dllColor4iv )(const GLint *v);
static void ( APIENTRY * dllColor4s )(GLshort red, GLshort green, GLshort blue, GLshort alpha);
static void ( APIENTRY * dllColor4sv )(const GLshort *v);
static void ( APIENTRY * dllColor4ub )(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
static void ( APIENTRY * dllColor4ubv )(const GLubyte *v);
static void ( APIENTRY * dllColor4ui )(GLuint red, GLuint green, GLuint blue, GLuint alpha);
static void ( APIENTRY * dllColor4uiv )(const GLuint *v);
static void ( APIENTRY * dllColor4us )(GLushort red, GLushort green, GLushort blue, GLushort alpha);
static void ( APIENTRY * dllColor4usv )(const GLushort *v);
static void ( APIENTRY * dllColorMask )(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
static void ( APIENTRY * dllColorMaterial )(GLenum face, GLenum mode);
static void ( APIENTRY * dllColorPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static void ( APIENTRY * dllCopyPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
static void ( APIENTRY * dllCopyTexImage1D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
static void ( APIENTRY * dllCopyTexImage2D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
static void ( APIENTRY * dllCopyTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
static void ( APIENTRY * dllCopyTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
static void ( APIENTRY * dllCullFace )(GLenum mode);
static void ( APIENTRY * dllDeleteLists )(GLuint list, GLsizei range);
static void ( APIENTRY * dllDeleteTextures )(GLsizei n, const GLuint *textures);
static void ( APIENTRY * dllDepthFunc )(GLenum func);
static void ( APIENTRY * dllDepthMask )(GLboolean flag);
static void ( APIENTRY * dllDepthRange )(GLclampd zNear, GLclampd zFar);
static void ( APIENTRY * dllDisable )(GLenum cap);
static void ( APIENTRY * dllDisableClientState )(GLenum array);
static void ( APIENTRY * dllDrawArrays )(GLenum mode, GLint first, GLsizei count);
static void ( APIENTRY * dllDrawBuffer )(GLenum mode);
static void ( APIENTRY * dllDrawElements )(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
static void ( APIENTRY * dllDrawPixels )(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
static void ( APIENTRY * dllEdgeFlag )(GLboolean flag);
static void ( APIENTRY * dllEdgeFlagPointer )(GLsizei stride, const GLvoid *pointer);
static void ( APIENTRY * dllEdgeFlagv )(const GLboolean *flag);
static void ( APIENTRY * dllEnable )(GLenum cap);
static void ( APIENTRY * dllEnableClientState )(GLenum array);
static void ( APIENTRY * dllEnd )(void);
static void ( APIENTRY * dllEndList )(void);
static void ( APIENTRY * dllEvalCoord1d )(GLdouble u);
static void ( APIENTRY * dllEvalCoord1dv )(const GLdouble *u);
static void ( APIENTRY * dllEvalCoord1f )(GLfloat u);
static void ( APIENTRY * dllEvalCoord1fv )(const GLfloat *u);
static void ( APIENTRY * dllEvalCoord2d )(GLdouble u, GLdouble v);
static void ( APIENTRY * dllEvalCoord2dv )(const GLdouble *u);
static void ( APIENTRY * dllEvalCoord2f )(GLfloat u, GLfloat v);
static void ( APIENTRY * dllEvalCoord2fv )(const GLfloat *u);
static void ( APIENTRY * dllEvalMesh1 )(GLenum mode, GLint i1, GLint i2);
static void ( APIENTRY * dllEvalMesh2 )(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
static void ( APIENTRY * dllEvalPoint1 )(GLint i);
static void ( APIENTRY * dllEvalPoint2 )(GLint i, GLint j);
static void ( APIENTRY * dllFeedbackBuffer )(GLsizei size, GLenum type, GLfloat *buffer);
static void ( APIENTRY * dllFinish )(void);
static void ( APIENTRY * dllFlush )(void);
static void ( APIENTRY * dllFogf )(GLenum pname, GLfloat param);
static void ( APIENTRY * dllFogfv )(GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllFogi )(GLenum pname, GLint param);
static void ( APIENTRY * dllFogiv )(GLenum pname, const GLint *params);
static void ( APIENTRY * dllFrontFace )(GLenum mode);
static void ( APIENTRY * dllFrustum )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint ( APIENTRY * dllGenLists )(GLsizei range);
static void ( APIENTRY * dllGenTextures )(GLsizei n, GLuint *textures);
static void ( APIENTRY * dllGetBooleanv )(GLenum pname, GLboolean *params);
static void ( APIENTRY * dllGetClipPlane )(GLenum plane, GLdouble *equation);
static void ( APIENTRY * dllGetDoublev )(GLenum pname, GLdouble *params);
GLenum ( APIENTRY * dllGetError )(void);
static void ( APIENTRY * dllGetFloatv )(GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetIntegerv )(GLenum pname, GLint *params);
static void ( APIENTRY * dllGetLightfv )(GLenum light, GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetLightiv )(GLenum light, GLenum pname, GLint *params);
static void ( APIENTRY * dllGetMapdv )(GLenum target, GLenum query, GLdouble *v);
static void ( APIENTRY * dllGetMapfv )(GLenum target, GLenum query, GLfloat *v);
static void ( APIENTRY * dllGetMapiv )(GLenum target, GLenum query, GLint *v);
static void ( APIENTRY * dllGetMaterialfv )(GLenum face, GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetMaterialiv )(GLenum face, GLenum pname, GLint *params);
static void ( APIENTRY * dllGetPixelMapfv )(GLenum map, GLfloat *values);
static void ( APIENTRY * dllGetPixelMapuiv )(GLenum map, GLuint *values);
static void ( APIENTRY * dllGetPixelMapusv )(GLenum map, GLushort *values);
static void ( APIENTRY * dllGetPointerv )(GLenum pname, GLvoid* *params);
static void ( APIENTRY * dllGetPolygonStipple )(GLubyte *mask);
const GLubyte * ( APIENTRY * dllGetString )(GLenum name);
static void ( APIENTRY * dllGetTexEnvfv )(GLenum target, GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetTexEnviv )(GLenum target, GLenum pname, GLint *params);
static void ( APIENTRY * dllGetTexGendv )(GLenum coord, GLenum pname, GLdouble *params);
static void ( APIENTRY * dllGetTexGenfv )(GLenum coord, GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetTexGeniv )(GLenum coord, GLenum pname, GLint *params);
static void ( APIENTRY * dllGetTexImage )(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
static void ( APIENTRY * dllGetTexLevelParameterfv )(GLenum target, GLint level, GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetTexLevelParameteriv )(GLenum target, GLint level, GLenum pname, GLint *params);
static void ( APIENTRY * dllGetTexParameterfv )(GLenum target, GLenum pname, GLfloat *params);
static void ( APIENTRY * dllGetTexParameteriv )(GLenum target, GLenum pname, GLint *params);
static void ( APIENTRY * dllHint )(GLenum target, GLenum mode);
static void ( APIENTRY * dllIndexMask )(GLuint mask);
static void ( APIENTRY * dllIndexPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
static void ( APIENTRY * dllIndexd )(GLdouble c);
static void ( APIENTRY * dllIndexdv )(const GLdouble *c);
static void ( APIENTRY * dllIndexf )(GLfloat c);
static void ( APIENTRY * dllIndexfv )(const GLfloat *c);
static void ( APIENTRY * dllIndexi )(GLint c);
static void ( APIENTRY * dllIndexiv )(const GLint *c);
static void ( APIENTRY * dllIndexs )(GLshort c);
static void ( APIENTRY * dllIndexsv )(const GLshort *c);
static void ( APIENTRY * dllIndexub )(GLubyte c);
static void ( APIENTRY * dllIndexubv )(const GLubyte *c);
static void ( APIENTRY * dllInitNames )(void);
static void ( APIENTRY * dllInterleavedArrays )(GLenum format, GLsizei stride, const GLvoid *pointer);
GLboolean ( APIENTRY * dllIsEnabled )(GLenum cap);
GLboolean ( APIENTRY * dllIsList )(GLuint list);
GLboolean ( APIENTRY * dllIsTexture )(GLuint texture);
static void ( APIENTRY * dllLightModelf )(GLenum pname, GLfloat param);
static void ( APIENTRY * dllLightModelfv )(GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllLightModeli )(GLenum pname, GLint param);
static void ( APIENTRY * dllLightModeliv )(GLenum pname, const GLint *params);
static void ( APIENTRY * dllLightf )(GLenum light, GLenum pname, GLfloat param);
static void ( APIENTRY * dllLightfv )(GLenum light, GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllLighti )(GLenum light, GLenum pname, GLint param);
static void ( APIENTRY * dllLightiv )(GLenum light, GLenum pname, const GLint *params);
static void ( APIENTRY * dllLineStipple )(GLint factor, GLushort pattern);
static void ( APIENTRY * dllLineWidth )(GLfloat width);
static void ( APIENTRY * dllListBase )(GLuint base);
static void ( APIENTRY * dllLoadIdentity )(void);
static void ( APIENTRY * dllLoadMatrixd )(const GLdouble *m);
static void ( APIENTRY * dllLoadMatrixf )(const GLfloat *m);
static void ( APIENTRY * dllLoadName )(GLuint name);
static void ( APIENTRY * dllLogicOp )(GLenum opcode);
static void ( APIENTRY * dllMap1d )(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
static void ( APIENTRY * dllMap1f )(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
static void ( APIENTRY * dllMap2d )(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
static void ( APIENTRY * dllMap2f )(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
static void ( APIENTRY * dllMapGrid1d )(GLint un, GLdouble u1, GLdouble u2);
static void ( APIENTRY * dllMapGrid1f )(GLint un, GLfloat u1, GLfloat u2);
static void ( APIENTRY * dllMapGrid2d )(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
static void ( APIENTRY * dllMapGrid2f )(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
static void ( APIENTRY * dllMaterialf )(GLenum face, GLenum pname, GLfloat param);
static void ( APIENTRY * dllMaterialfv )(GLenum face, GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllMateriali )(GLenum face, GLenum pname, GLint param);
static void ( APIENTRY * dllMaterialiv )(GLenum face, GLenum pname, const GLint *params);
static void ( APIENTRY * dllMatrixMode )(GLenum mode);
static void ( APIENTRY * dllMultMatrixd )(const GLdouble *m);
static void ( APIENTRY * dllMultMatrixf )(const GLfloat *m);
static void ( APIENTRY * dllNewList )(GLuint list, GLenum mode);
static void ( APIENTRY * dllNormal3b )(GLbyte nx, GLbyte ny, GLbyte nz);
static void ( APIENTRY * dllNormal3bv )(const GLbyte *v);
static void ( APIENTRY * dllNormal3d )(GLdouble nx, GLdouble ny, GLdouble nz);
static void ( APIENTRY * dllNormal3dv )(const GLdouble *v);
static void ( APIENTRY * dllNormal3f )(GLfloat nx, GLfloat ny, GLfloat nz);
static void ( APIENTRY * dllNormal3fv )(const GLfloat *v);
static void ( APIENTRY * dllNormal3i )(GLint nx, GLint ny, GLint nz);
static void ( APIENTRY * dllNormal3iv )(const GLint *v);
static void ( APIENTRY * dllNormal3s )(GLshort nx, GLshort ny, GLshort nz);
static void ( APIENTRY * dllNormal3sv )(const GLshort *v);
static void ( APIENTRY * dllNormalPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
static void ( APIENTRY * dllOrtho )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
static void ( APIENTRY * dllPassThrough )(GLfloat token);
static void ( APIENTRY * dllPixelMapfv )(GLenum map, GLsizei mapsize, const GLfloat *values);
static void ( APIENTRY * dllPixelMapuiv )(GLenum map, GLsizei mapsize, const GLuint *values);
static void ( APIENTRY * dllPixelMapusv )(GLenum map, GLsizei mapsize, const GLushort *values);
static void ( APIENTRY * dllPixelStoref )(GLenum pname, GLfloat param);
static void ( APIENTRY * dllPixelStorei )(GLenum pname, GLint param);
static void ( APIENTRY * dllPixelTransferf )(GLenum pname, GLfloat param);
static void ( APIENTRY * dllPixelTransferi )(GLenum pname, GLint param);
static void ( APIENTRY * dllPixelZoom )(GLfloat xfactor, GLfloat yfactor);
static void ( APIENTRY * dllPointSize )(GLfloat size);
static void ( APIENTRY * dllPolygonMode )(GLenum face, GLenum mode);
static void ( APIENTRY * dllPolygonOffset )(GLfloat factor, GLfloat units);
static void ( APIENTRY * dllPolygonStipple )(const GLubyte *mask);
static void ( APIENTRY * dllPopAttrib )(void);
static void ( APIENTRY * dllPopClientAttrib )(void);
static void ( APIENTRY * dllPopMatrix )(void);
static void ( APIENTRY * dllPopName )(void);
static void ( APIENTRY * dllPrioritizeTextures )(GLsizei n, const GLuint *textures, const GLclampf *priorities);
static void ( APIENTRY * dllPushAttrib )(GLbitfield mask);
static void ( APIENTRY * dllPushClientAttrib )(GLbitfield mask);
static void ( APIENTRY * dllPushMatrix )(void);
static void ( APIENTRY * dllPushName )(GLuint name);
static void ( APIENTRY * dllRasterPos2d )(GLdouble x, GLdouble y);
static void ( APIENTRY * dllRasterPos2dv )(const GLdouble *v);
static void ( APIENTRY * dllRasterPos2f )(GLfloat x, GLfloat y);
static void ( APIENTRY * dllRasterPos2fv )(const GLfloat *v);
static void ( APIENTRY * dllRasterPos2i )(GLint x, GLint y);
static void ( APIENTRY * dllRasterPos2iv )(const GLint *v);
static void ( APIENTRY * dllRasterPos2s )(GLshort x, GLshort y);
static void ( APIENTRY * dllRasterPos2sv )(const GLshort *v);
static void ( APIENTRY * dllRasterPos3d )(GLdouble x, GLdouble y, GLdouble z);
static void ( APIENTRY * dllRasterPos3dv )(const GLdouble *v);
static void ( APIENTRY * dllRasterPos3f )(GLfloat x, GLfloat y, GLfloat z);
static void ( APIENTRY * dllRasterPos3fv )(const GLfloat *v);
static void ( APIENTRY * dllRasterPos3i )(GLint x, GLint y, GLint z);
static void ( APIENTRY * dllRasterPos3iv )(const GLint *v);
static void ( APIENTRY * dllRasterPos3s )(GLshort x, GLshort y, GLshort z);
static void ( APIENTRY * dllRasterPos3sv )(const GLshort *v);
static void ( APIENTRY * dllRasterPos4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
static void ( APIENTRY * dllRasterPos4dv )(const GLdouble *v);
static void ( APIENTRY * dllRasterPos4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
static void ( APIENTRY * dllRasterPos4fv )(const GLfloat *v);
static void ( APIENTRY * dllRasterPos4i )(GLint x, GLint y, GLint z, GLint w);
static void ( APIENTRY * dllRasterPos4iv )(const GLint *v);
static void ( APIENTRY * dllRasterPos4s )(GLshort x, GLshort y, GLshort z, GLshort w);
static void ( APIENTRY * dllRasterPos4sv )(const GLshort *v);
static void ( APIENTRY * dllReadBuffer )(GLenum mode);
static void ( APIENTRY * dllReadPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
static void ( APIENTRY * dllRectd )(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
static void ( APIENTRY * dllRectdv )(const GLdouble *v1, const GLdouble *v2);
static void ( APIENTRY * dllRectf )(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
static void ( APIENTRY * dllRectfv )(const GLfloat *v1, const GLfloat *v2);
static void ( APIENTRY * dllRecti )(GLint x1, GLint y1, GLint x2, GLint y2);
static void ( APIENTRY * dllRectiv )(const GLint *v1, const GLint *v2);
static void ( APIENTRY * dllRects )(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
static void ( APIENTRY * dllRectsv )(const GLshort *v1, const GLshort *v2);
GLint ( APIENTRY * dllRenderMode )(GLenum mode);
static void ( APIENTRY * dllRotated )(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
static void ( APIENTRY * dllRotatef )(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
static void ( APIENTRY * dllScaled )(GLdouble x, GLdouble y, GLdouble z);
static void ( APIENTRY * dllScalef )(GLfloat x, GLfloat y, GLfloat z);
static void ( APIENTRY * dllScissor )(GLint x, GLint y, GLsizei width, GLsizei height);
static void ( APIENTRY * dllSelectBuffer )(GLsizei size, GLuint *buffer);
static void ( APIENTRY * dllShadeModel )(GLenum mode);
static void ( APIENTRY * dllStencilFunc )(GLenum func, GLint ref, GLuint mask);
static void ( APIENTRY * dllStencilMask )(GLuint mask);
static void ( APIENTRY * dllStencilOp )(GLenum fail, GLenum zfail, GLenum zpass);
static void ( APIENTRY * dllTexCoord1d )(GLdouble s);
static void ( APIENTRY * dllTexCoord1dv )(const GLdouble *v);
static void ( APIENTRY * dllTexCoord1f )(GLfloat s);
static void ( APIENTRY * dllTexCoord1fv )(const GLfloat *v);
static void ( APIENTRY * dllTexCoord1i )(GLint s);
static void ( APIENTRY * dllTexCoord1iv )(const GLint *v);
static void ( APIENTRY * dllTexCoord1s )(GLshort s);
static void ( APIENTRY * dllTexCoord1sv )(const GLshort *v);
static void ( APIENTRY * dllTexCoord2d )(GLdouble s, GLdouble t);
static void ( APIENTRY * dllTexCoord2dv )(const GLdouble *v);
static void ( APIENTRY * dllTexCoord2f )(GLfloat s, GLfloat t);
static void ( APIENTRY * dllTexCoord2fv )(const GLfloat *v);
static void ( APIENTRY * dllTexCoord2i )(GLint s, GLint t);
static void ( APIENTRY * dllTexCoord2iv )(const GLint *v);
static void ( APIENTRY * dllTexCoord2s )(GLshort s, GLshort t);
static void ( APIENTRY * dllTexCoord2sv )(const GLshort *v);
static void ( APIENTRY * dllTexCoord3d )(GLdouble s, GLdouble t, GLdouble r);
static void ( APIENTRY * dllTexCoord3dv )(const GLdouble *v);
static void ( APIENTRY * dllTexCoord3f )(GLfloat s, GLfloat t, GLfloat r);
static void ( APIENTRY * dllTexCoord3fv )(const GLfloat *v);
static void ( APIENTRY * dllTexCoord3i )(GLint s, GLint t, GLint r);
static void ( APIENTRY * dllTexCoord3iv )(const GLint *v);
static void ( APIENTRY * dllTexCoord3s )(GLshort s, GLshort t, GLshort r);
static void ( APIENTRY * dllTexCoord3sv )(const GLshort *v);
static void ( APIENTRY * dllTexCoord4d )(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
static void ( APIENTRY * dllTexCoord4dv )(const GLdouble *v);
static void ( APIENTRY * dllTexCoord4f )(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
static void ( APIENTRY * dllTexCoord4fv )(const GLfloat *v);
static void ( APIENTRY * dllTexCoord4i )(GLint s, GLint t, GLint r, GLint q);
static void ( APIENTRY * dllTexCoord4iv )(const GLint *v);
static void ( APIENTRY * dllTexCoord4s )(GLshort s, GLshort t, GLshort r, GLshort q);
static void ( APIENTRY * dllTexCoord4sv )(const GLshort *v);
static void ( APIENTRY * dllTexCoordPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static void ( APIENTRY * dllTexEnvf )(GLenum target, GLenum pname, GLfloat param);
static void ( APIENTRY * dllTexEnvfv )(GLenum target, GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllTexEnvi )(GLenum target, GLenum pname, GLint param);
static void ( APIENTRY * dllTexEnviv )(GLenum target, GLenum pname, const GLint *params);
static void ( APIENTRY * dllTexGend )(GLenum coord, GLenum pname, GLdouble param);
static void ( APIENTRY * dllTexGendv )(GLenum coord, GLenum pname, const GLdouble *params);
static void ( APIENTRY * dllTexGenf )(GLenum coord, GLenum pname, GLfloat param);
static void ( APIENTRY * dllTexGenfv )(GLenum coord, GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllTexGeni )(GLenum coord, GLenum pname, GLint param);
static void ( APIENTRY * dllTexGeniv )(GLenum coord, GLenum pname, const GLint *params);
static void ( APIENTRY * dllTexImage1D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static void ( APIENTRY * dllTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static void ( APIENTRY * dllTexParameterf )(GLenum target, GLenum pname, GLfloat param);
static void ( APIENTRY * dllTexParameterfv )(GLenum target, GLenum pname, const GLfloat *params);
static void ( APIENTRY * dllTexParameteri )(GLenum target, GLenum pname, GLint param);
static void ( APIENTRY * dllTexParameteriv )(GLenum target, GLenum pname, const GLint *params);
static void ( APIENTRY * dllTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
static void ( APIENTRY * dllTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
static void ( APIENTRY * dllTranslated )(GLdouble x, GLdouble y, GLdouble z);
static void ( APIENTRY * dllTranslatef )(GLfloat x, GLfloat y, GLfloat z);
static void ( APIENTRY * dllVertex2d )(GLdouble x, GLdouble y);
static void ( APIENTRY * dllVertex2dv )(const GLdouble *v);
static void ( APIENTRY * dllVertex2f )(GLfloat x, GLfloat y);
static void ( APIENTRY * dllVertex2fv )(const GLfloat *v);
static void ( APIENTRY * dllVertex2i )(GLint x, GLint y);
static void ( APIENTRY * dllVertex2iv )(const GLint *v);
static void ( APIENTRY * dllVertex2s )(GLshort x, GLshort y);
static void ( APIENTRY * dllVertex2sv )(const GLshort *v);
static void ( APIENTRY * dllVertex3d )(GLdouble x, GLdouble y, GLdouble z);
static void ( APIENTRY * dllVertex3dv )(const GLdouble *v);
static void ( APIENTRY * dllVertex3f )(GLfloat x, GLfloat y, GLfloat z);
static void ( APIENTRY * dllVertex3fv )(const GLfloat *v);
static void ( APIENTRY * dllVertex3i )(GLint x, GLint y, GLint z);
static void ( APIENTRY * dllVertex3iv )(const GLint *v);
static void ( APIENTRY * dllVertex3s )(GLshort x, GLshort y, GLshort z);
static void ( APIENTRY * dllVertex3sv )(const GLshort *v);
static void ( APIENTRY * dllVertex4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
static void ( APIENTRY * dllVertex4dv )(const GLdouble *v);
static void ( APIENTRY * dllVertex4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
static void ( APIENTRY * dllVertex4fv )(const GLfloat *v);
static void ( APIENTRY * dllVertex4i )(GLint x, GLint y, GLint z, GLint w);
static void ( APIENTRY * dllVertex4iv )(const GLint *v);
static void ( APIENTRY * dllVertex4s )(GLshort x, GLshort y, GLshort z, GLshort w);
static void ( APIENTRY * dllVertex4sv )(const GLshort *v);
static void ( APIENTRY * dllVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
static void ( APIENTRY * dllViewport )(GLint x, GLint y, GLsizei width, GLsizei height);

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown( void )
{
	if ( glw_state.hinstOpenGL )
	{
		FreeLibrary( glw_state.hinstOpenGL );
		glw_state.hinstOpenGL = NULL;
	}

	glw_state.hinstOpenGL = NULL;

	qglAccum                     = NULL;
	qglAlphaFunc                 = NULL;
	qglAreTexturesResident       = NULL;
	qglArrayElement              = NULL;
	qglBegin                     = NULL;
	qglBindTexture               = NULL;
	qglBitmap                    = NULL;
	qglBlendFunc                 = NULL;
	qglCallList                  = NULL;
	qglCallLists                 = NULL;
	qglClear                     = NULL;
	qglClearAccum                = NULL;
	qglClearColor                = NULL;
	qglClearDepth                = NULL;
	qglClearIndex                = NULL;
	qglClearStencil              = NULL;
	qglClipPlane                 = NULL;
	qglColor3b                   = NULL;
	qglColor3bv                  = NULL;
	qglColor3d                   = NULL;
	qglColor3dv                  = NULL;
	qglColor3f                   = NULL;
	qglColor3fv                  = NULL;
	qglColor3i                   = NULL;
	qglColor3iv                  = NULL;
	qglColor3s                   = NULL;
	qglColor3sv                  = NULL;
	qglColor3ub                  = NULL;
	qglColor3ubv                 = NULL;
	qglColor3ui                  = NULL;
	qglColor3uiv                 = NULL;
	qglColor3us                  = NULL;
	qglColor3usv                 = NULL;
	qglColor4b                   = NULL;
	qglColor4bv                  = NULL;
	qglColor4d                   = NULL;
	qglColor4dv                  = NULL;
	qglColor4f                   = NULL;
	qglColor4fv                  = NULL;
	qglColor4i                   = NULL;
	qglColor4iv                  = NULL;
	qglColor4s                   = NULL;
	qglColor4sv                  = NULL;
	qglColor4ub                  = NULL;
	qglColor4ubv                 = NULL;
	qglColor4ui                  = NULL;
	qglColor4uiv                 = NULL;
	qglColor4us                  = NULL;
	qglColor4usv                 = NULL;
	qglColorMask                 = NULL;
	qglColorMaterial             = NULL;
	qglColorPointer              = NULL;
	qglCopyPixels                = NULL;
	qglCopyTexImage1D            = NULL;
	qglCopyTexImage2D            = NULL;
	qglCopyTexSubImage1D         = NULL;
	qglCopyTexSubImage2D         = NULL;
	qglCullFace                  = NULL;
	qglDeleteLists               = NULL;
	qglDeleteTextures            = NULL;
	qglDepthFunc                 = NULL;
	qglDepthMask                 = NULL;
	qglDepthRange                = NULL;
	qglDisable                   = NULL;
	qglDisableClientState        = NULL;
	qglDrawArrays                = NULL;
	qglDrawBuffer                = NULL;
	qglDrawElements              = NULL;
	qglDrawPixels                = NULL;
	qglEdgeFlag                  = NULL;
	qglEdgeFlagPointer           = NULL;
	qglEdgeFlagv                 = NULL;
	qglEnable                    = NULL;
	qglEnableClientState         = NULL;
	qglEnd                       = NULL;
	qglEndList                   = NULL;
	/*qglEvalCoord1d               = NULL;
	qglEvalCoord1dv              = NULL;
	qglEvalCoord1f               = NULL;
	qglEvalCoord1fv              = NULL;
	qglEvalCoord2d               = NULL;
	qglEvalCoord2dv              = NULL;
	qglEvalCoord2f               = NULL;
	qglEvalCoord2fv              = NULL;
	qglEvalMesh1                 = NULL;
	qglEvalMesh2                 = NULL;
	qglEvalPoint1                = NULL;
	qglEvalPoint2                = NULL;*/
	qglFeedbackBuffer            = NULL;
	qglFinish                    = NULL;
	qglFlush                     = NULL;
	qglFogf                      = NULL;
	qglFogfv                     = NULL;
	qglFogi                      = NULL;
	qglFogiv                     = NULL;
	qglFrontFace                 = NULL;
	qglFrustum                   = NULL;
	qglGenLists                  = NULL;
	qglGenTextures               = NULL;
	qglGetBooleanv               = NULL;
	qglGetClipPlane              = NULL;
	qglGetDoublev                = NULL;
	qglGetError                  = NULL;
	qglGetFloatv                 = NULL;
	qglGetIntegerv               = NULL;
	qglGetLightfv                = NULL;
	qglGetLightiv                = NULL;
	qglGetMapdv                  = NULL;
	qglGetMapfv                  = NULL;
	qglGetMapiv                  = NULL;
	qglGetMaterialfv             = NULL;
	qglGetMaterialiv             = NULL;
	qglGetPixelMapfv             = NULL;
	qglGetPixelMapuiv            = NULL;
	qglGetPixelMapusv            = NULL;
	qglGetPointerv               = NULL;
	qglGetPolygonStipple         = NULL;
	qglGetString                 = NULL;
	qglGetTexEnvfv               = NULL;
	qglGetTexEnviv               = NULL;
	qglGetTexGendv               = NULL;
	qglGetTexGenfv               = NULL;
	qglGetTexGeniv               = NULL;
	qglGetTexImage               = NULL;
	//qglGetTexLevelParameterfv    = NULL;
	//qglGetTexLevelParameteriv    = NULL;
	qglGetTexParameterfv         = NULL;
	qglGetTexParameteriv         = NULL;
	qglHint                      = NULL;
	qglIndexMask                 = NULL;
	qglIndexPointer              = NULL;
	qglIndexd                    = NULL;
	qglIndexdv                   = NULL;
	qglIndexf                    = NULL;
	qglIndexfv                   = NULL;
	qglIndexi                    = NULL;
	qglIndexiv                   = NULL;
	qglIndexs                    = NULL;
	qglIndexsv                   = NULL;
	qglIndexub                   = NULL;
	qglIndexubv                  = NULL;
	qglInitNames                 = NULL;
	qglInterleavedArrays         = NULL;
	qglIsEnabled                 = NULL;
	qglIsList                    = NULL;
	qglIsTexture                 = NULL;
	qglLightModelf               = NULL;
	qglLightModelfv              = NULL;
	qglLightModeli               = NULL;
	qglLightModeliv              = NULL;
	qglLightf                    = NULL;
	qglLightfv                   = NULL;
	qglLighti                    = NULL;
	qglLightiv                   = NULL;
	qglLineStipple               = NULL;
	qglLineWidth                 = NULL;
	qglListBase                  = NULL;
	qglLoadIdentity              = NULL;
	qglLoadMatrixd               = NULL;
	qglLoadMatrixf               = NULL;
	qglLoadName                  = NULL;
	qglLogicOp                   = NULL;
	/*qglMap1d                     = NULL;
	qglMap1f                     = NULL;
	qglMap2d                     = NULL;
	qglMap2f                     = NULL;
	qglMapGrid1d                 = NULL;
	qglMapGrid1f                 = NULL;
	qglMapGrid2d                 = NULL;
	qglMapGrid2f                 = NULL;
	qglMaterialf                 = NULL;
	qglMaterialfv                = NULL;
	qglMateriali                 = NULL;
	qglMaterialiv                = NULL;*/
	qglMatrixMode                = NULL;
	qglMultMatrixd               = NULL;
	qglMultMatrixf               = NULL;
	qglNewList                   = NULL;
	qglNormal3b                  = NULL;
	qglNormal3bv                 = NULL;
	qglNormal3d                  = NULL;
	qglNormal3dv                 = NULL;
	qglNormal3f                  = NULL;
	qglNormal3fv                 = NULL;
	qglNormal3i                  = NULL;
	qglNormal3iv                 = NULL;
	qglNormal3s                  = NULL;
	qglNormal3sv                 = NULL;
	qglNormalPointer             = NULL;
	qglOrtho                     = NULL;
	qglPassThrough               = NULL;
	qglPixelMapfv                = NULL;
	qglPixelMapuiv               = NULL;
	qglPixelMapusv               = NULL;
	qglPixelStoref               = NULL;
	qglPixelStorei               = NULL;
	qglPixelTransferf            = NULL;
	qglPixelTransferi            = NULL;
	qglPixelZoom                 = NULL;
	qglPointSize                 = NULL;
	qglPolygonMode               = NULL;
	qglPolygonOffset             = NULL;
	qglPolygonStipple            = NULL;
	qglPopAttrib                 = NULL;
	qglPopClientAttrib           = NULL;
	qglPopMatrix                 = NULL;
	qglPopName                   = NULL;
	qglPrioritizeTextures        = NULL;
	qglPushAttrib                = NULL;
	qglPushClientAttrib          = NULL;
	qglPushMatrix                = NULL;
	qglPushName                  = NULL;
	/*qglRasterPos2d               = NULL;
	qglRasterPos2dv              = NULL;
	qglRasterPos2f               = NULL;
	qglRasterPos2fv              = NULL;
	qglRasterPos2i               = NULL;
	qglRasterPos2iv              = NULL;
	qglRasterPos2s               = NULL;
	qglRasterPos2sv              = NULL;
	qglRasterPos3d               = NULL;
	qglRasterPos3dv              = NULL;
	qglRasterPos3f               = NULL;
	qglRasterPos3fv              = NULL;
	qglRasterPos3i               = NULL;
	qglRasterPos3iv              = NULL;
	qglRasterPos3s               = NULL;
	qglRasterPos3sv              = NULL;
	qglRasterPos4d               = NULL;
	qglRasterPos4dv              = NULL;
	qglRasterPos4f               = NULL;
	qglRasterPos4fv              = NULL;
	qglRasterPos4i               = NULL;
	qglRasterPos4iv              = NULL;
	qglRasterPos4s               = NULL;
	qglRasterPos4sv              = NULL;*/
	qglReadBuffer                = NULL;
	qglReadPixels                = NULL;
	qglRectd                     = NULL;
	qglRectdv                    = NULL;
	qglRectf                     = NULL;
	qglRectfv                    = NULL;
	qglRecti                     = NULL;
	qglRectiv                    = NULL;
	qglRects                     = NULL;
	qglRectsv                    = NULL;
	qglRenderMode                = NULL;
	qglRotated                   = NULL;
	qglRotatef                   = NULL;
	qglScaled                    = NULL;
	qglScalef                    = NULL;
	qglScissor                   = NULL;
	qglSelectBuffer              = NULL;
	qglShadeModel                = NULL;
	qglStencilFunc               = NULL;
	qglStencilMask               = NULL;
	qglStencilOp                 = NULL;
	qglTexCoord1d                = NULL;
	qglTexCoord1dv               = NULL;
	qglTexCoord1f                = NULL;
	qglTexCoord1fv               = NULL;
	qglTexCoord1i                = NULL;
	qglTexCoord1iv               = NULL;
	qglTexCoord1s                = NULL;
	qglTexCoord1sv               = NULL;
	qglTexCoord2d                = NULL;
	qglTexCoord2dv               = NULL;
	qglTexCoord2f                = NULL;
	qglTexCoord2fv               = NULL;
	qglTexCoord2i                = NULL;
	qglTexCoord2iv               = NULL;
	qglTexCoord2s                = NULL;
	qglTexCoord2sv               = NULL;
	qglTexCoord3d                = NULL;
	qglTexCoord3dv               = NULL;
	qglTexCoord3f                = NULL;
	qglTexCoord3fv               = NULL;
	qglTexCoord3i                = NULL;
	qglTexCoord3iv               = NULL;
	qglTexCoord3s                = NULL;
	qglTexCoord3sv               = NULL;
	qglTexCoord4d                = NULL;
	qglTexCoord4dv               = NULL;
	qglTexCoord4f                = NULL;
	qglTexCoord4fv               = NULL;
	qglTexCoord4i                = NULL;
	qglTexCoord4iv               = NULL;
	qglTexCoord4s                = NULL;
	qglTexCoord4sv               = NULL;
	qglTexCoordPointer           = NULL;
	qglTexEnvf                   = NULL;
	qglTexEnvfv                  = NULL;
	qglTexEnvi                   = NULL;
	qglTexEnviv                  = NULL;
	qglTexGend                   = NULL;
	qglTexGendv                  = NULL;
	qglTexGenf                   = NULL;
	qglTexGenfv                  = NULL;
	qglTexGeni                   = NULL;
	qglTexGeniv                  = NULL;
	qglTexImage1D                = NULL;
	qglTexImage2D                = NULL;
	qglTexParameterf             = NULL;
	qglTexParameterfv            = NULL;
	qglTexParameteri             = NULL;
	qglTexParameteriv            = NULL;
	qglTexSubImage1D             = NULL;
	qglTexSubImage2D             = NULL;
	qglTranslated                = NULL;
	qglTranslatef                = NULL;
	qglVertex2d                  = NULL;
	qglVertex2dv                 = NULL;
	qglVertex2f                  = NULL;
	qglVertex2fv                 = NULL;
	qglVertex2i                  = NULL;
	qglVertex2iv                 = NULL;
	qglVertex2s                  = NULL;
	qglVertex2sv                 = NULL;
	qglVertex3d                  = NULL;
	qglVertex3dv                 = NULL;
	qglVertex3f                  = NULL;
	qglVertex3fv                 = NULL;
	qglVertex3i                  = NULL;
	qglVertex3iv                 = NULL;
	qglVertex3s                  = NULL;
	qglVertex3sv                 = NULL;
	qglVertex4d                  = NULL;
	qglVertex4dv                 = NULL;
	qglVertex4f                  = NULL;
	qglVertex4fv                 = NULL;
	qglVertex4i                  = NULL;
	qglVertex4iv                 = NULL;
	qglVertex4s                  = NULL;
	qglVertex4sv                 = NULL;
	qglVertexPointer             = NULL;
	qglViewport                  = NULL;

	qwglCopyContext              = NULL;
	qwglCreateContext            = NULL;
	qwglCreateLayerContext       = NULL;
	qwglDeleteContext            = NULL;
	qwglDescribeLayerPlane       = NULL;
	qwglGetCurrentContext        = NULL;
	qwglGetCurrentDC             = NULL;
	qwglGetLayerPaletteEntries   = NULL;
	qwglGetProcAddress           = NULL;
	qwglMakeCurrent              = NULL;
	qwglRealizeLayerPalette      = NULL;
	qwglSetLayerPaletteEntries   = NULL;
	qwglShareLists               = NULL;
	qwglSwapLayerBuffers         = NULL;
	qwglUseFontBitmaps           = NULL;
	qwglUseFontOutlines          = NULL;

	qwglChoosePixelFormat        = NULL;
	qwglDescribePixelFormat      = NULL;
	qwglGetPixelFormat           = NULL;
	qwglSetPixelFormat           = NULL;
	qwglSwapBuffers              = NULL;

	qwglSwapIntervalEXT	= NULL;

	qwglGetDeviceGammaRampEXT = NULL;
	qwglSetDeviceGammaRampEXT = NULL;
}

#	pragma warning (disable : 4113 4133 4047 )
#	define GPA( a ) GetProcAddress( glw_state.hinstOpenGL, a )
#ifdef _DEBUG
char isenabled[1048576];
void __stdcall R1LogEnable (GLenum caps)
{
	if (isenabled[caps] == 55)
	{
		ri.Con_Printf (PRINT_ALL, "already enabled\n");
		//return;
	}
	isenabled[caps] = 55;
	dllEnable (caps);
}

void __stdcall R1LogDisable (GLenum caps)
{
	if (isenabled[caps] == 66)
	{
		ri.Con_Printf (PRINT_ALL, "already disabled\n");
		//return;
	}
	isenabled[caps] = 66;
	dllDisable (caps);
}
#endif

#ifdef R1GL_RELEASE

void GetVersionOfFile (char * pszAppName, // file
                       char * pInternal, // receives version
					   char * pName,
					   char * pVersion,
                       char * pszLangBuff) // size of buffer
{
   DWORD dwScratch;
   DWORD * pdwLangChar;
   DWORD dwInfSize ;
   UINT uSize;
   BYTE * pbyInfBuff;
   char szVersion [32];
   char szResource [80];
   char * pszVersion = szVersion;

   dwInfSize = GetFileVersionInfoSize (pszAppName, &dwScratch);

   if (dwInfSize)
   {
      pbyInfBuff = malloc(dwInfSize);
      memset (pbyInfBuff, 0, dwInfSize);
      if (pbyInfBuff)
      {
         if (GetFileVersionInfo (pszAppName, 0, dwInfSize, pbyInfBuff))
         {
            if (VerQueryValue (pbyInfBuff,
                               "\\VarFileInfo\\Translation",
                               (void**)(&pdwLangChar),
                               &uSize))
            {
               if (VerLanguageName (LOWORD (*pdwLangChar),
                                    szResource,
                                    sizeof(szResource)))
               {
                  strcpy (pszLangBuff, szResource);
               }
               wsprintf (szResource, "\\StringFileInfo\\%04X%04X\\ProductName",
                         LOWORD (*pdwLangChar), HIWORD (*pdwLangChar));

               if (VerQueryValue (pbyInfBuff,
                                  szResource,
                                  (void**)(&pszVersion),
                                  &uSize))
               {
                  strcpy (pName, pszVersion);
               }

               wsprintf (szResource, "\\StringFileInfo\\%04X%04X\\InternalName",
                         LOWORD (*pdwLangChar), HIWORD (*pdwLangChar));

               if (VerQueryValue (pbyInfBuff,
                                  szResource,
                                  (void**)(&pszVersion),
                                  &uSize))
               {
                  strcpy (pInternal, pszVersion);
               }

               wsprintf (szResource, "\\StringFileInfo\\%04X%04X\\FileVersion",
                         LOWORD (*pdwLangChar), HIWORD (*pdwLangChar));

               if (VerQueryValue (pbyInfBuff,
                                  szResource,
                                  (void**)(&pszVersion),
                                  &uSize))
               {
                  strcpy (pVersion, pszVersion);
               }
			}
         }
         free (pbyInfBuff);
      }
   }
}

//stupid hack to verify opengl driver, tiny hint of cheat protection...
//blah blah yes its obfuscated in ASM so idiots can't go hey number + 5
//trivial to break or delete should you want to :).

void OGL_Version_f (void)
{
	char message[1024];
	char mybuff[16];
	char driverName[MAX_PATH];
	int len, verifyNum, x, crc;
	char name[64], version[64], language[64], internal[64];
	GetModuleFileName (glw_state.hinstOpenGL, driverName, sizeof(driverName)-1);
	
	x = atoi(ri.Cmd_Argv (1));

	strcpy (name, "[NO OS]");
	strcpy (version, "[NO VERSION]");
	strcpy (language, "[NO LANGUAGE]");
	strcpy (internal, "[NO NAME]");

	GetVersionOfFile (driverName, internal, name, version, language);
	{
		FILE *t = fopen (driverName, "rb");
		if (t) {
			byte *k;
			fseek (t, 0, SEEK_END);
			len = ftell (t);
			k = malloc(len);
			fseek (t, 0, SEEK_SET);
			fread (k, len, 1, t);
			crc = crc32 (0, k, len);
			free (k);
			fclose (t);
		} else {
			len = -1;
			crc = 0x00BADBAD;
		}
	}

	if (!x) {
		verifyNum = 0;
	} else {
		_asm {
			mov         eax,dword ptr [x] 
			imul        eax,eax,3F86h 
			mov         ecx,dword ptr [len] 
			imul        ecx,ecx,1852h 
			lea         edx,[eax+ecx+0DEADh] 
			mov         dword ptr [verifyNum],edx 
		}
	}

	sprintf (mybuff, " /%d/", verifyNum);
	snprintf (message, sizeof(message)-1, "cmd say \"%s: %s %s[%s], v%s (%08lx), %d bytes%s.\"\n", driverName, name, internal, language, version, crc, len, verifyNum ? mybuff : "");
	ri.Cmd_ExecuteText (EXEC_APPEND, message);
}

#endif
/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to 
** the appropriate GL stuff.  In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
** 
*/
qboolean QGL_Init( const char *dllname )
{
	// update 3Dfx gamma irrespective of underlying DLL
	{
		char envbuffer[1024];
		float g;

		g = 2.00 * ( 0.8 - ( vid_gamma->value - 0.5 ) ) + 1.0F;
		Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g );
		putenv( envbuffer );
		Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g );
		putenv( envbuffer );
	}

#ifdef R1GL_RELEASE
	ri.Cmd_AddCommand ("opengl_version", OGL_Version_f);
#endif

	if ( ( glw_state.hinstOpenGL = LoadLibrary( dllname ) ) == 0 )
	{
		char *buf = NULL;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		ri.Con_Printf( PRINT_ALL, "%s\n", buf );
		return false;
	}

	gl_config.allow_cds = true;

	qglAccum                     = dllAccum = GPA( "glAccum" );
	qglAlphaFunc                 = dllAlphaFunc = GPA( "glAlphaFunc" );
	qglAreTexturesResident       = dllAreTexturesResident = GPA( "glAreTexturesResident" );
	qglArrayElement              = dllArrayElement = GPA( "glArrayElement" );
	qglBegin                     = dllBegin = GPA( "glBegin" );
	qglBindTexture               = dllBindTexture = GPA( "glBindTexture" );
	qglBitmap                    = dllBitmap = GPA( "glBitmap" );
	qglBlendFunc                 = dllBlendFunc = GPA( "glBlendFunc" );
	qglCallList                  = dllCallList = GPA( "glCallList" );
	qglCallLists                 = dllCallLists = GPA( "glCallLists" );
	qglClear                     = dllClear = GPA( "glClear" );
	qglClearAccum                = dllClearAccum = GPA( "glClearAccum" );
	qglClearColor                = dllClearColor = GPA( "glClearColor" );
	qglClearDepth                = dllClearDepth = GPA( "glClearDepth" );
	qglClearIndex                = dllClearIndex = GPA( "glClearIndex" );
	qglClearStencil              = dllClearStencil = GPA( "glClearStencil" );
	qglClipPlane                 = dllClipPlane = GPA( "glClipPlane" );
	qglColor3b                   = dllColor3b = GPA( "glColor3b" );
	qglColor3bv                  = dllColor3bv = GPA( "glColor3bv" );
	qglColor3d                   = dllColor3d = GPA( "glColor3d" );
	qglColor3dv                  = dllColor3dv = GPA( "glColor3dv" );
	qglColor3f                   = dllColor3f = GPA( "glColor3f" );
	qglColor3fv                  = dllColor3fv = GPA( "glColor3fv" );
	qglColor3i                   = dllColor3i = GPA( "glColor3i" );
	qglColor3iv                  = dllColor3iv = GPA( "glColor3iv" );
	qglColor3s                   = dllColor3s = GPA( "glColor3s" );
	qglColor3sv                  = dllColor3sv = GPA( "glColor3sv" );
	qglColor3ub                  = dllColor3ub = GPA( "glColor3ub" );
	qglColor3ubv                 = dllColor3ubv = GPA( "glColor3ubv" );
	qglColor3ui                  = dllColor3ui = GPA( "glColor3ui" );
	qglColor3uiv                 = dllColor3uiv = GPA( "glColor3uiv" );
	qglColor3us                  = dllColor3us = GPA( "glColor3us" );
	qglColor3usv                 = dllColor3usv = GPA( "glColor3usv" );
	qglColor4b                   = dllColor4b = GPA( "glColor4b" );
	qglColor4bv                  = dllColor4bv = GPA( "glColor4bv" );
	qglColor4d                   = dllColor4d = GPA( "glColor4d" );
	qglColor4dv                  = dllColor4dv = GPA( "glColor4dv" );
	qglColor4f                   = dllColor4f = GPA( "glColor4f" );
	qglColor4fv                  = dllColor4fv = GPA( "glColor4fv" );
	qglColor4i                   = dllColor4i = GPA( "glColor4i" );
	qglColor4iv                  = dllColor4iv = GPA( "glColor4iv" );
	qglColor4s                   = dllColor4s = GPA( "glColor4s" );
	qglColor4sv                  = dllColor4sv = GPA( "glColor4sv" );
	qglColor4ub                  = dllColor4ub = GPA( "glColor4ub" );
	qglColor4ubv                 = dllColor4ubv = GPA( "glColor4ubv" );
	qglColor4ui                  = dllColor4ui = GPA( "glColor4ui" );
	qglColor4uiv                 = dllColor4uiv = GPA( "glColor4uiv" );
	qglColor4us                  = dllColor4us = GPA( "glColor4us" );
	qglColor4usv                 = dllColor4usv = GPA( "glColor4usv" );
	qglColorMask                 = dllColorMask = GPA( "glColorMask" );
	qglColorMaterial             = dllColorMaterial = GPA( "glColorMaterial" );
	qglColorPointer              = dllColorPointer = GPA( "glColorPointer" );
	qglCopyPixels                = dllCopyPixels = GPA( "glCopyPixels" );
	qglCopyTexImage1D            = dllCopyTexImage1D = GPA( "glCopyTexImage1D" );
	qglCopyTexImage2D            = dllCopyTexImage2D = GPA( "glCopyTexImage2D" );
	qglCopyTexSubImage1D         = dllCopyTexSubImage1D = GPA( "glCopyTexSubImage1D" );
	qglCopyTexSubImage2D         = dllCopyTexSubImage2D = GPA( "glCopyTexSubImage2D" );
	qglCullFace                  = dllCullFace = GPA( "glCullFace" );
	qglDeleteLists               = dllDeleteLists = GPA( "glDeleteLists" );
	qglDeleteTextures            = dllDeleteTextures = GPA( "glDeleteTextures" );
	qglDepthFunc                 = dllDepthFunc = GPA( "glDepthFunc" );
	qglDepthMask                 = dllDepthMask = GPA( "glDepthMask" );
	qglDepthRange                = dllDepthRange = GPA( "glDepthRange" );
	qglDisable                   = dllDisable = GPA( "glDisable" );
	/*qglDisable = R1LogDisable;
	dllDisable = GPA ("glDisable");*/
	qglDisableClientState        = dllDisableClientState = GPA( "glDisableClientState" );
	qglDrawArrays                = dllDrawArrays = GPA( "glDrawArrays" );
	qglDrawBuffer                = dllDrawBuffer = GPA( "glDrawBuffer" );
	qglDrawElements              = dllDrawElements = GPA( "glDrawElements" );
	qglDrawPixels                = dllDrawPixels = GPA( "glDrawPixels" );
	qglEdgeFlag                  = dllEdgeFlag = GPA( "glEdgeFlag" );
	qglEdgeFlagPointer           = dllEdgeFlagPointer = GPA( "glEdgeFlagPointer" );
	qglEdgeFlagv                 = dllEdgeFlagv = GPA( "glEdgeFlagv" );
	qglEnable                    = 	dllEnable                    = GPA( "glEnable" );
	/*qglEnable = R1LogEnable;
	dllEnable = GPA( "glEnable" );*/
	qglEnableClientState         = 	dllEnableClientState         = GPA( "glEnableClientState" );
	qglEnd                       = 	dllEnd                       = GPA( "glEnd" );
	qglEndList                   = 	dllEndList                   = GPA( "glEndList" );
	/*qglEvalCoord1d				 = 	dllEvalCoord1d				 = GPA( "glEvalCoord1d" );
	qglEvalCoord1dv              = 	dllEvalCoord1dv              = GPA( "glEvalCoord1dv" );
	qglEvalCoord1f               = 	dllEvalCoord1f               = GPA( "glEvalCoord1f" );
	qglEvalCoord1fv              = 	dllEvalCoord1fv              = GPA( "glEvalCoord1fv" );
	qglEvalCoord2d               = 	dllEvalCoord2d               = GPA( "glEvalCoord2d" );
	qglEvalCoord2dv              = 	dllEvalCoord2dv              = GPA( "glEvalCoord2dv" );
	qglEvalCoord2f               = 	dllEvalCoord2f               = GPA( "glEvalCoord2f" );
	qglEvalCoord2fv              = 	dllEvalCoord2fv              = GPA( "glEvalCoord2fv" );
	qglEvalMesh1                 = 	dllEvalMesh1                 = GPA( "glEvalMesh1" );
	qglEvalMesh2                 = 	dllEvalMesh2                 = GPA( "glEvalMesh2" );
	qglEvalPoint1                = 	dllEvalPoint1                = GPA( "glEvalPoint1" );
	qglEvalPoint2                = 	dllEvalPoint2                = GPA( "glEvalPoint2" );*/
	qglFeedbackBuffer            = 	dllFeedbackBuffer            = GPA( "glFeedbackBuffer" );
	qglFinish                    = 	dllFinish                    = GPA( "glFinish" );
	qglFlush                     = 	dllFlush                     = GPA( "glFlush" );
	qglFogf                      = 	dllFogf                      = GPA( "glFogf" );
	qglFogfv                     = 	dllFogfv                     = GPA( "glFogfv" );
	qglFogi                      = 	dllFogi                      = GPA( "glFogi" );
	qglFogiv                     = 	dllFogiv                     = GPA( "glFogiv" );
	qglFrontFace                 = 	dllFrontFace                 = GPA( "glFrontFace" );
	qglFrustum                   = 	dllFrustum                   = GPA( "glFrustum" );
	qglGenLists                  = 	dllGenLists                  = GPA( "glGenLists" );
	qglGenTextures               = 	dllGenTextures               = GPA( "glGenTextures" );
	qglGetBooleanv               = 	dllGetBooleanv               = GPA( "glGetBooleanv" );
	qglGetClipPlane              = 	dllGetClipPlane              = GPA( "glGetClipPlane" );
	qglGetDoublev                = 	dllGetDoublev                = GPA( "glGetDoublev" );
	qglGetError                  = 	dllGetError                  = GPA( "glGetError" );
	qglGetFloatv                 = 	dllGetFloatv                 = GPA( "glGetFloatv" );
	qglGetIntegerv               = 	dllGetIntegerv               = GPA( "glGetIntegerv" );
	qglGetLightfv                = 	dllGetLightfv                = GPA( "glGetLightfv" );
	qglGetLightiv                = 	dllGetLightiv                = GPA( "glGetLightiv" );
	qglGetMapdv                  = 	dllGetMapdv                  = GPA( "glGetMapdv" );
	qglGetMapfv                  = 	dllGetMapfv                  = GPA( "glGetMapfv" );
	qglGetMapiv                  = 	dllGetMapiv                  = GPA( "glGetMapiv" );
	qglGetMaterialfv             = 	dllGetMaterialfv             = GPA( "glGetMaterialfv" );
	qglGetMaterialiv             = 	dllGetMaterialiv             = GPA( "glGetMaterialiv" );
	qglGetPixelMapfv             = 	dllGetPixelMapfv             = GPA( "glGetPixelMapfv" );
	qglGetPixelMapuiv            = 	dllGetPixelMapuiv            = GPA( "glGetPixelMapuiv" );
	qglGetPixelMapusv            = 	dllGetPixelMapusv            = GPA( "glGetPixelMapusv" );
	qglGetPointerv               = 	dllGetPointerv               = GPA( "glGetPointerv" );
	qglGetPolygonStipple         = 	dllGetPolygonStipple         = GPA( "glGetPolygonStipple" );
	qglGetString                 = 	dllGetString                 = GPA( "glGetString" );
	qglGetTexEnvfv               = 	dllGetTexEnvfv               = GPA( "glGetTexEnvfv" );
	qglGetTexEnviv               = 	dllGetTexEnviv               = GPA( "glGetTexEnviv" );
	qglGetTexGendv               = 	dllGetTexGendv               = GPA( "glGetTexGendv" );
	qglGetTexGenfv               = 	dllGetTexGenfv               = GPA( "glGetTexGenfv" );
	qglGetTexGeniv               = 	dllGetTexGeniv               = GPA( "glGetTexGeniv" );
	qglGetTexImage               = 	dllGetTexImage               = GPA( "glGetTexImage" );
	//qglGetTexLevelParameterfv    = 	dllGetTexLevelParameterfv    = GPA( "glGetLevelParameterfv" );
	//qglGetTexLevelParameteriv    = 	dllGetTexLevelParameteriv    = GPA( "glGetLevelParameteriv" );
	qglGetTexParameterfv         = 	dllGetTexParameterfv         = GPA( "glGetTexParameterfv" );
	qglGetTexParameteriv         = 	dllGetTexParameteriv         = GPA( "glGetTexParameteriv" );
	qglHint                      = 	dllHint                      = GPA( "glHint" );
	qglIndexMask                 = 	dllIndexMask                 = GPA( "glIndexMask" );
	qglIndexPointer              = 	dllIndexPointer              = GPA( "glIndexPointer" );
	qglIndexd                    = 	dllIndexd                    = GPA( "glIndexd" );
	qglIndexdv                   = 	dllIndexdv                   = GPA( "glIndexdv" );
	qglIndexf                    = 	dllIndexf                    = GPA( "glIndexf" );
	qglIndexfv                   = 	dllIndexfv                   = GPA( "glIndexfv" );
	qglIndexi                    = 	dllIndexi                    = GPA( "glIndexi" );
	qglIndexiv                   = 	dllIndexiv                   = GPA( "glIndexiv" );
	qglIndexs                    = 	dllIndexs                    = GPA( "glIndexs" );
	qglIndexsv                   = 	dllIndexsv                   = GPA( "glIndexsv" );
	qglIndexub                   = 	dllIndexub                   = GPA( "glIndexub" );
	qglIndexubv                  = 	dllIndexubv                  = GPA( "glIndexubv" );
	qglInitNames                 = 	dllInitNames                 = GPA( "glInitNames" );
	qglInterleavedArrays         = 	dllInterleavedArrays         = GPA( "glInterleavedArrays" );
	qglIsEnabled                 = 	dllIsEnabled                 = GPA( "glIsEnabled" );
	qglIsList                    = 	dllIsList                    = GPA( "glIsList" );
	qglIsTexture                 = 	dllIsTexture                 = GPA( "glIsTexture" );
	qglLightModelf               = 	dllLightModelf               = GPA( "glLightModelf" );
	qglLightModelfv              = 	dllLightModelfv              = GPA( "glLightModelfv" );
	qglLightModeli               = 	dllLightModeli               = GPA( "glLightModeli" );
	qglLightModeliv              = 	dllLightModeliv              = GPA( "glLightModeliv" );
	qglLightf                    = 	dllLightf                    = GPA( "glLightf" );
	qglLightfv                   = 	dllLightfv                   = GPA( "glLightfv" );
	qglLighti                    = 	dllLighti                    = GPA( "glLighti" );
	qglLightiv                   = 	dllLightiv                   = GPA( "glLightiv" );
	qglLineStipple               = 	dllLineStipple               = GPA( "glLineStipple" );
	qglLineWidth                 = 	dllLineWidth                 = GPA( "glLineWidth" );
	qglListBase                  = 	dllListBase                  = GPA( "glListBase" );
	qglLoadIdentity              = 	dllLoadIdentity              = GPA( "glLoadIdentity" );
	qglLoadMatrixd               = 	dllLoadMatrixd               = GPA( "glLoadMatrixd" );
	qglLoadMatrixf               = 	dllLoadMatrixf               = GPA( "glLoadMatrixf" );
	qglLoadName                  = 	dllLoadName                  = GPA( "glLoadName" );
	qglLogicOp                   = 	dllLogicOp                   = GPA( "glLogicOp" );
	/*qglMap1d                     = 	dllMap1d                     = GPA( "glMap1d" );
	qglMap1f                     = 	dllMap1f                     = GPA( "glMap1f" );
	qglMap2d                     = 	dllMap2d                     = GPA( "glMap2d" );
	qglMap2f                     = 	dllMap2f                     = GPA( "glMap2f" );
	qglMapGrid1d                 = 	dllMapGrid1d                 = GPA( "glMapGrid1d" );
	qglMapGrid1f                 = 	dllMapGrid1f                 = GPA( "glMapGrid1f" );
	qglMapGrid2d                 = 	dllMapGrid2d                 = GPA( "glMapGrid2d" );
	qglMapGrid2f                 = 	dllMapGrid2f                 = GPA( "glMapGrid2f" );
	qglMaterialf                 = 	dllMaterialf                 = GPA( "glMaterialf" );
	qglMaterialfv                = 	dllMaterialfv                = GPA( "glMaterialfv" );
	qglMateriali                 = 	dllMateriali                 = GPA( "glMateriali" );
	qglMaterialiv                = 	dllMaterialiv                = GPA( "glMaterialiv" );*/
	qglMatrixMode                = 	dllMatrixMode                = GPA( "glMatrixMode" );
	qglMultMatrixd               = 	dllMultMatrixd               = GPA( "glMultMatrixd" );
	qglMultMatrixf               = 	dllMultMatrixf               = GPA( "glMultMatrixf" );
	qglNewList                   = 	dllNewList                   = GPA( "glNewList" );
	qglNormal3b                  = 	dllNormal3b                  = GPA( "glNormal3b" );
	qglNormal3bv                 = 	dllNormal3bv                 = GPA( "glNormal3bv" );
	qglNormal3d                  = 	dllNormal3d                  = GPA( "glNormal3d" );
	qglNormal3dv                 = 	dllNormal3dv                 = GPA( "glNormal3dv" );
	qglNormal3f                  = 	dllNormal3f                  = GPA( "glNormal3f" );
	qglNormal3fv                 = 	dllNormal3fv                 = GPA( "glNormal3fv" );
	qglNormal3i                  = 	dllNormal3i                  = GPA( "glNormal3i" );
	qglNormal3iv                 = 	dllNormal3iv                 = GPA( "glNormal3iv" );
	qglNormal3s                  = 	dllNormal3s                  = GPA( "glNormal3s" );
	qglNormal3sv                 = 	dllNormal3sv                 = GPA( "glNormal3sv" );
	qglNormalPointer             = 	dllNormalPointer             = GPA( "glNormalPointer" );
	qglOrtho                     = 	dllOrtho                     = GPA( "glOrtho" );
	qglPassThrough               = 	dllPassThrough               = GPA( "glPassThrough" );
	qglPixelMapfv                = 	dllPixelMapfv                = GPA( "glPixelMapfv" );
	qglPixelMapuiv               = 	dllPixelMapuiv               = GPA( "glPixelMapuiv" );
	qglPixelMapusv               = 	dllPixelMapusv               = GPA( "glPixelMapusv" );
	qglPixelStoref               = 	dllPixelStoref               = GPA( "glPixelStoref" );
	qglPixelStorei               = 	dllPixelStorei               = GPA( "glPixelStorei" );
	qglPixelTransferf            = 	dllPixelTransferf            = GPA( "glPixelTransferf" );
	qglPixelTransferi            = 	dllPixelTransferi            = GPA( "glPixelTransferi" );
	qglPixelZoom                 = 	dllPixelZoom                 = GPA( "glPixelZoom" );
	qglPointSize                 = 	dllPointSize                 = GPA( "glPointSize" );
	qglPolygonMode               = 	dllPolygonMode               = GPA( "glPolygonMode" );
	qglPolygonOffset             = 	dllPolygonOffset             = GPA( "glPolygonOffset" );
	qglPolygonStipple            = 	dllPolygonStipple            = GPA( "glPolygonStipple" );
	qglPopAttrib                 = 	dllPopAttrib                 = GPA( "glPopAttrib" );
	qglPopClientAttrib           = 	dllPopClientAttrib           = GPA( "glPopClientAttrib" );
	qglPopMatrix                 = 	dllPopMatrix                 = GPA( "glPopMatrix" );
	qglPopName                   = 	dllPopName                   = GPA( "glPopName" );
	qglPrioritizeTextures        = 	dllPrioritizeTextures        = GPA( "glPrioritizeTextures" );
	qglPushAttrib                = 	dllPushAttrib                = GPA( "glPushAttrib" );
	qglPushClientAttrib          = 	dllPushClientAttrib          = GPA( "glPushClientAttrib" );
	qglPushMatrix                = 	dllPushMatrix                = GPA( "glPushMatrix" );
	qglPushName                  = 	dllPushName                  = GPA( "glPushName" );
	/*qglRasterPos2d               = 	dllRasterPos2d               = GPA( "glRasterPos2d" );
	qglRasterPos2dv              = 	dllRasterPos2dv              = GPA( "glRasterPos2dv" );
	qglRasterPos2f               = 	dllRasterPos2f               = GPA( "glRasterPos2f" );
	qglRasterPos2fv              = 	dllRasterPos2fv              = GPA( "glRasterPos2fv" );
	qglRasterPos2i               = 	dllRasterPos2i               = GPA( "glRasterPos2i" );
	qglRasterPos2iv              = 	dllRasterPos2iv              = GPA( "glRasterPos2iv" );
	qglRasterPos2s               = 	dllRasterPos2s               = GPA( "glRasterPos2s" );
	qglRasterPos2sv              = 	dllRasterPos2sv              = GPA( "glRasterPos2sv" );
	qglRasterPos3d               = 	dllRasterPos3d               = GPA( "glRasterPos3d" );
	qglRasterPos3dv              = 	dllRasterPos3dv              = GPA( "glRasterPos3dv" );
	qglRasterPos3f               = 	dllRasterPos3f               = GPA( "glRasterPos3f" );
	qglRasterPos3fv              = 	dllRasterPos3fv              = GPA( "glRasterPos3fv" );
	qglRasterPos3i               = 	dllRasterPos3i               = GPA( "glRasterPos3i" );
	qglRasterPos3iv              = 	dllRasterPos3iv              = GPA( "glRasterPos3iv" );
	qglRasterPos3s               = 	dllRasterPos3s               = GPA( "glRasterPos3s" );
	qglRasterPos3sv              = 	dllRasterPos3sv              = GPA( "glRasterPos3sv" );
	qglRasterPos4d               = 	dllRasterPos4d               = GPA( "glRasterPos4d" );
	qglRasterPos4dv              = 	dllRasterPos4dv              = GPA( "glRasterPos4dv" );
	qglRasterPos4f               = 	dllRasterPos4f               = GPA( "glRasterPos4f" );
	qglRasterPos4fv              = 	dllRasterPos4fv              = GPA( "glRasterPos4fv" );
	qglRasterPos4i               = 	dllRasterPos4i               = GPA( "glRasterPos4i" );
	qglRasterPos4iv              = 	dllRasterPos4iv              = GPA( "glRasterPos4iv" );
	qglRasterPos4s               = 	dllRasterPos4s               = GPA( "glRasterPos4s" );
	qglRasterPos4sv              = 	dllRasterPos4sv              = GPA( "glRasterPos4sv" );*/
	qglReadBuffer                = 	dllReadBuffer                = GPA( "glReadBuffer" );
	qglReadPixels                = 	dllReadPixels                = GPA( "glReadPixels" );
	qglRectd                     = 	dllRectd                     = GPA( "glRectd" );
	qglRectdv                    = 	dllRectdv                    = GPA( "glRectdv" );
	qglRectf                     = 	dllRectf                     = GPA( "glRectf" );
	qglRectfv                    = 	dllRectfv                    = GPA( "glRectfv" );
	qglRecti                     = 	dllRecti                     = GPA( "glRecti" );
	qglRectiv                    = 	dllRectiv                    = GPA( "glRectiv" );
	qglRects                     = 	dllRects                     = GPA( "glRects" );
	qglRectsv                    = 	dllRectsv                    = GPA( "glRectsv" );
	qglRenderMode                = 	dllRenderMode                = GPA( "glRenderMode" );
	qglRotated                   = 	dllRotated                   = GPA( "glRotated" );
	qglRotatef                   = 	dllRotatef                   = GPA( "glRotatef" );
	qglScaled                    = 	dllScaled                    = GPA( "glScaled" );
	qglScalef                    = 	dllScalef                    = GPA( "glScalef" );
	qglScissor                   = 	dllScissor                   = GPA( "glScissor" );
	qglSelectBuffer              = 	dllSelectBuffer              = GPA( "glSelectBuffer" );
	qglShadeModel                = 	dllShadeModel                = GPA( "glShadeModel" );
	qglStencilFunc               = 	dllStencilFunc               = GPA( "glStencilFunc" );
	qglStencilMask               = 	dllStencilMask               = GPA( "glStencilMask" );
	qglStencilOp                 = 	dllStencilOp                 = GPA( "glStencilOp" );
	qglTexCoord1d                = 	dllTexCoord1d                = GPA( "glTexCoord1d" );
	qglTexCoord1dv               = 	dllTexCoord1dv               = GPA( "glTexCoord1dv" );
	qglTexCoord1f                = 	dllTexCoord1f                = GPA( "glTexCoord1f" );
	qglTexCoord1fv               = 	dllTexCoord1fv               = GPA( "glTexCoord1fv" );
	qglTexCoord1i                = 	dllTexCoord1i                = GPA( "glTexCoord1i" );
	qglTexCoord1iv               = 	dllTexCoord1iv               = GPA( "glTexCoord1iv" );
	qglTexCoord1s                = 	dllTexCoord1s                = GPA( "glTexCoord1s" );
	qglTexCoord1sv               = 	dllTexCoord1sv               = GPA( "glTexCoord1sv" );
	qglTexCoord2d                = 	dllTexCoord2d                = GPA( "glTexCoord2d" );
	qglTexCoord2dv               = 	dllTexCoord2dv               = GPA( "glTexCoord2dv" );
	qglTexCoord2f                = 	dllTexCoord2f                = GPA( "glTexCoord2f" );
	qglTexCoord2fv               = 	dllTexCoord2fv               = GPA( "glTexCoord2fv" );
	qglTexCoord2i                = 	dllTexCoord2i                = GPA( "glTexCoord2i" );
	qglTexCoord2iv               = 	dllTexCoord2iv               = GPA( "glTexCoord2iv" );
	qglTexCoord2s                = 	dllTexCoord2s                = GPA( "glTexCoord2s" );
	qglTexCoord2sv               = 	dllTexCoord2sv               = GPA( "glTexCoord2sv" );
	qglTexCoord3d                = 	dllTexCoord3d                = GPA( "glTexCoord3d" );
	qglTexCoord3dv               = 	dllTexCoord3dv               = GPA( "glTexCoord3dv" );
	qglTexCoord3f                = 	dllTexCoord3f                = GPA( "glTexCoord3f" );
	qglTexCoord3fv               = 	dllTexCoord3fv               = GPA( "glTexCoord3fv" );
	qglTexCoord3i                = 	dllTexCoord3i                = GPA( "glTexCoord3i" );
	qglTexCoord3iv               = 	dllTexCoord3iv               = GPA( "glTexCoord3iv" );
	qglTexCoord3s                = 	dllTexCoord3s                = GPA( "glTexCoord3s" );
	qglTexCoord3sv               = 	dllTexCoord3sv               = GPA( "glTexCoord3sv" );
	qglTexCoord4d                = 	dllTexCoord4d                = GPA( "glTexCoord4d" );
	qglTexCoord4dv               = 	dllTexCoord4dv               = GPA( "glTexCoord4dv" );
	qglTexCoord4f                = 	dllTexCoord4f                = GPA( "glTexCoord4f" );
	qglTexCoord4fv               = 	dllTexCoord4fv               = GPA( "glTexCoord4fv" );
	qglTexCoord4i                = 	dllTexCoord4i                = GPA( "glTexCoord4i" );
	qglTexCoord4iv               = 	dllTexCoord4iv               = GPA( "glTexCoord4iv" );
	qglTexCoord4s                = 	dllTexCoord4s                = GPA( "glTexCoord4s" );
	qglTexCoord4sv               = 	dllTexCoord4sv               = GPA( "glTexCoord4sv" );
	qglTexCoordPointer           = 	dllTexCoordPointer           = GPA( "glTexCoordPointer" );
	qglTexEnvf                   = 	dllTexEnvf                   = GPA( "glTexEnvf" );
	qglTexEnvfv                  = 	dllTexEnvfv                  = GPA( "glTexEnvfv" );
	qglTexEnvi                   = 	dllTexEnvi                   = GPA( "glTexEnvi" );
	qglTexEnviv                  = 	dllTexEnviv                  = GPA( "glTexEnviv" );
	qglTexGend                   = 	dllTexGend                   = GPA( "glTexGend" );
	qglTexGendv                  = 	dllTexGendv                  = GPA( "glTexGendv" );
	qglTexGenf                   = 	dllTexGenf                   = GPA( "glTexGenf" );
	qglTexGenfv                  = 	dllTexGenfv                  = GPA( "glTexGenfv" );
	qglTexGeni                   = 	dllTexGeni                   = GPA( "glTexGeni" );
	qglTexGeniv                  = 	dllTexGeniv                  = GPA( "glTexGeniv" );
	qglTexImage1D                = 	dllTexImage1D                = GPA( "glTexImage1D" );
	qglTexImage2D                = 	dllTexImage2D                = GPA( "glTexImage2D" );
	qglTexParameterf             = 	dllTexParameterf             = GPA( "glTexParameterf" );
	qglTexParameterfv            = 	dllTexParameterfv            = GPA( "glTexParameterfv" );
	qglTexParameteri             = 	dllTexParameteri             = GPA( "glTexParameteri" );
	qglTexParameteriv            = 	dllTexParameteriv            = GPA( "glTexParameteriv" );
	qglTexSubImage1D             = 	dllTexSubImage1D             = GPA( "glTexSubImage1D" );
	qglTexSubImage2D             = 	dllTexSubImage2D             = GPA( "glTexSubImage2D" );
	qglTranslated                = 	dllTranslated                = GPA( "glTranslated" );
	qglTranslatef                = 	dllTranslatef                = GPA( "glTranslatef" );
	qglVertex2d                  = 	dllVertex2d                  = GPA( "glVertex2d" );
	qglVertex2dv                 = 	dllVertex2dv                 = GPA( "glVertex2dv" );
	qglVertex2f                  = 	dllVertex2f                  = GPA( "glVertex2f" );
	qglVertex2fv                 = 	dllVertex2fv                 = GPA( "glVertex2fv" );
	qglVertex2i                  = 	dllVertex2i                  = GPA( "glVertex2i" );
	qglVertex2iv                 = 	dllVertex2iv                 = GPA( "glVertex2iv" );
	qglVertex2s                  = 	dllVertex2s                  = GPA( "glVertex2s" );
	qglVertex2sv                 = 	dllVertex2sv                 = GPA( "glVertex2sv" );
	qglVertex3d                  = 	dllVertex3d                  = GPA( "glVertex3d" );
	qglVertex3dv                 = 	dllVertex3dv                 = GPA( "glVertex3dv" );
	qglVertex3f                  = 	dllVertex3f                  = GPA( "glVertex3f" );
	qglVertex3fv                 = 	dllVertex3fv                 = GPA( "glVertex3fv" );
	qglVertex3i                  = 	dllVertex3i                  = GPA( "glVertex3i" );
	qglVertex3iv                 = 	dllVertex3iv                 = GPA( "glVertex3iv" );
	qglVertex3s                  = 	dllVertex3s                  = GPA( "glVertex3s" );
	qglVertex3sv                 = 	dllVertex3sv                 = GPA( "glVertex3sv" );
	qglVertex4d                  = 	dllVertex4d                  = GPA( "glVertex4d" );
	qglVertex4dv                 = 	dllVertex4dv                 = GPA( "glVertex4dv" );
	qglVertex4f                  = 	dllVertex4f                  = GPA( "glVertex4f" );
	qglVertex4fv                 = 	dllVertex4fv                 = GPA( "glVertex4fv" );
	qglVertex4i                  = 	dllVertex4i                  = GPA( "glVertex4i" );
	qglVertex4iv                 = 	dllVertex4iv                 = GPA( "glVertex4iv" );
	qglVertex4s                  = 	dllVertex4s                  = GPA( "glVertex4s" );
	qglVertex4sv                 = 	dllVertex4sv                 = GPA( "glVertex4sv" );
	qglVertexPointer             = 	dllVertexPointer             = GPA( "glVertexPointer" );
	qglViewport                  = 	dllViewport                  = GPA( "glViewport" );

	qwglCopyContext              = GPA( "wglCopyContext" );
	qwglCreateContext            = GPA( "wglCreateContext" );
	qwglCreateLayerContext       = GPA( "wglCreateLayerContext" );
	qwglDeleteContext            = GPA( "wglDeleteContext" );
	qwglDescribeLayerPlane       = GPA( "wglDescribeLayerPlane" );
	qwglGetCurrentContext        = GPA( "wglGetCurrentContext" );
	qwglGetCurrentDC             = GPA( "wglGetCurrentDC" );
	qwglGetLayerPaletteEntries   = GPA( "wglGetLayerPaletteEntries" );
	qwglGetProcAddress           = GPA( "wglGetProcAddress" );
	qwglMakeCurrent              = GPA( "wglMakeCurrent" );
	qwglRealizeLayerPalette      = GPA( "wglRealizeLayerPalette" );
	qwglSetLayerPaletteEntries   = GPA( "wglSetLayerPaletteEntries" );
	qwglShareLists               = GPA( "wglShareLists" );
	qwglSwapLayerBuffers         = GPA( "wglSwapLayerBuffers" );
	qwglUseFontBitmaps           = GPA( "wglUseFontBitmapsA" );
	qwglUseFontOutlines          = GPA( "wglUseFontOutlinesA" );

	qwglChoosePixelFormat        = GPA( "wglChoosePixelFormat" );
	qwglDescribePixelFormat      = GPA( "wglDescribePixelFormat" );
	qwglGetPixelFormat           = GPA( "wglGetPixelFormat" );
	qwglSetPixelFormat           = GPA( "wglSetPixelFormat" );
	qwglSwapBuffers              = GPA( "wglSwapBuffers" );

	qwglSwapIntervalEXT = NULL;
	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	qglColorTableEXT = NULL;
	qglSelectTextureSGIS = NULL;
	qglMTexCoord2fSGIS = NULL;
	qglMTexCoord2fvSGIS = NULL;

	return true;
}

#pragma warning (default : 4113 4133 4047 )

