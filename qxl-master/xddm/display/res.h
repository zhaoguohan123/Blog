/*
   Copyright (C) 2009 Red Hat, Inc.

   This software is licensed under the GNU General Public License,
   version 2 (GPLv2) (see COPYING for details), subject to the
   following clarification.

   With respect to binaries built using the Microsoft(R) Windows
   Driver Kit (WDK), GPLv2 does not extend to any code contained in or
   derived from the WDK ("WDK Code").  As to WDK Code, by using or
   distributing such binaries you agree to be bound by the Microsoft
   Software License Terms for the WDK.  All WDK Code is considered by
   the GPLv2 licensors to qualify for the special exception stated in
   section 3 of GPLv2 (commonly known as the system library
   exception).

   There is NO WARRANTY for this software, express or implied,
   including the implied warranties of NON-INFRINGEMENT, TITLE,
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef _H_RES
#define _H_RES

#include "qxldd.h"

UINT64 ReleaseOutput(PDev *pdev, UINT64 output_id);

QXLDrawable *Drawable(PDev *pdev, UINT8 type, RECTL *area, CLIPOBJ *clip, UINT32 surface_id);
void PushDrawable(PDev *pdev, QXLDrawable *drawable);
QXLSurfaceCmd *SurfaceCmd(PDev *pdev, UINT8 type, UINT32 surface_id);
void PushSurfaceCmd(PDev *pdev, QXLSurfaceCmd *surface_cmd);

QXLPHYSICAL SurfaceToPhysical(PDev *pdev, UINT8 *base_mem);
void QXLGetSurface(PDev *pdev, QXLPHYSICAL *surface_phys, UINT32 x, UINT32 y, UINT32 depth,
                   INT32 *stride, UINT8 **base_mem, UINT8 allocation_type);
void QXLGetDelSurface(PDev *pdev, QXLSurfaceCmd *surface, UINT32 surface_id, UINT8 allocation_type);
void QXLDelSurface(PDev *pdev, UINT8 *base_mem, UINT8 allocation_type);
BOOL QXLGetPath(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *path_phys, PATHOBJ *path);
BOOL QXLGetMask(PDev *pdev, QXLDrawable *drawable, QXLQMask *qxl_mask, SURFOBJ *mask, POINTL *pos,
                BOOL invers, LONG width, LONG height, INT32 *surface_dest);
BOOL QXLGetBrush(PDev *pdev, QXLDrawable *drawable, QXLBrush *qxl_brush,
                            BRUSHOBJ *brush, POINTL *brush_pos, INT32 *surface_dest,
                            QXLRect *surface_rect);
BOOL QXLGetBitmap(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *image_phys, SURFOBJ *surf,
                  QXLRect *area, XLATEOBJ *color_trans, UINT32 *hash_key, BOOL use_cache,
                  INT32 *surface_dest);
BOOL QXLGetBitsFromCache(PDev *pdev, QXLDrawable *drawable, UINT32 hash_key, QXLPHYSICAL *image_phys);
BOOL QXLGetAlphaBitmap(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *image_phys, SURFOBJ *surf,
                       QXLRect *area, INT32 *surface_dest, XLATEOBJ *color_trans);
BOOL QXLCheckIfCacheImage(PDev *pdev, SURFOBJ *surf, XLATEOBJ *color_trans);
UINT8 *QXLGetBuf(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *buf_phys, UINT32 size);
BOOL QXLGetStr(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *str_phys, FONTOBJ *font, STROBJ *str);

void UpdateArea(PDev *pdev, RECTL *area, UINT32 surface_id);

QXLCursorCmd *CursorCmd(PDev *pdev);
void PushCursorCmd(PDev *pdev, QXLCursorCmd *cursor_cmd);

BOOL GetAlphaCursor(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf);
BOOL GetColorCursor(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf,
                    SURFOBJ *mask, XLATEOBJ *color_trans);
BOOL GetMonoCursor(PDev *pdev, QXLCursorCmd *cmd, LONG hot_x, LONG hot_y, SURFOBJ *surf);
BOOL GetTransparentCursor(PDev *pdev, QXLCursorCmd *cmd);

BOOL ResInit(PDev *pdev);
void ResDestroy(PDev *pdev);
void ResInitGlobals();
void ResDestroyGlobals();
#ifndef _WIN64
void CheckAndSetSSE2();
#endif
void EmptyReleaseRing(PDev *pdev);
void InitDeviceMemoryResources(PDev *pdev);
void ReleaseCacheDeviceMemoryResources(PDev *pdev);

#endif
