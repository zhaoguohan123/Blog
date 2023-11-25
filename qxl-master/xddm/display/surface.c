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

#include "stddef.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "os_dep.h"

#include "winerror.h"
#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "devioctl.h"
#include "ntddvdeo.h"

#include "qxldd.h"
#include "utils.h"
#include "mspace.h"
#include "res.h"
#include "surface.h"

static BOOL CreateDrawArea(PDev *pdev, UINT8 *base_mem, ULONG format, UINT32 cx, UINT32 cy,
                           UINT32 stride, UINT32 surface_id)
{
    SIZEL  size;
    DrawArea *drawarea;

    size.cx = cx;
    size.cy = cy;

    drawarea = &GetSurfaceInfo(pdev, surface_id)->draw_area;

    if (!(drawarea->bitmap = (HSURF)EngCreateBitmap(size, stride, format, 0, base_mem))) {
        DEBUG_PRINT((pdev, 0, "%s: EngCreateBitmap failed\n", __FUNCTION__));
        return FALSE;
    }

    if (!EngAssociateSurface(drawarea->bitmap, pdev->eng, 0)) {
        DEBUG_PRINT((pdev, 0, "%s: EngAssociateSurface failed\n", __FUNCTION__));
        goto error;
    }

    if (!(drawarea->surf_obj = EngLockSurface(drawarea->bitmap))) {
        DEBUG_PRINT((pdev, 0, "%s: EngLockSurface failed\n", __FUNCTION__));
        goto error;
    }

    drawarea->base_mem = base_mem;

    return TRUE;
error:
    EngDeleteSurface(drawarea->bitmap);
    return FALSE;
}

static VOID FreeDrawArea(DrawArea *drawarea)
{
    if (drawarea->surf_obj) {
        EngUnlockSurface(drawarea->surf_obj);
        EngDeleteSurface(drawarea->bitmap);
        drawarea->surf_obj = NULL;
    }
}

static void BitmapFormatToDepthAndSurfaceFormat(ULONG format, UINT32 *depth, UINT32 *surface_format)
{
    switch (format) {
        case BMF_16BPP:
            *surface_format = SPICE_SURFACE_FMT_16_555;
            *depth = 16;
            break;
        case BMF_24BPP:
        case BMF_32BPP:
            *surface_format = SPICE_SURFACE_FMT_32_xRGB;
            *depth = 32;
            break;
        default:
            *depth = 0;
            break;
    };
}

static UINT8 *CreateSurfaceHelper(PDev *pdev, UINT32 surface_id,
                                  UINT32 cx, UINT32 cy, ULONG format,
                                  UINT8 allocation_type,
                                  INT32 *stride, UINT32 *surface_format,
                                  QXLPHYSICAL *phys_mem)
{
    UINT32 depth;
    SurfaceInfo *surface_info = GetSurfaceInfo(pdev, surface_id);
    UINT8 *base_mem;
    int size;

    BitmapFormatToDepthAndSurfaceFormat(format, &depth, surface_format);
    ASSERT(pdev, depth != 0);
    ASSERT(pdev, stride);
    QXLGetSurface(pdev, phys_mem, cx, cy, depth, stride, &base_mem, allocation_type);
    DEBUG_PRINT((pdev, 3,
        "%s: %d, pm %0lX, fmt %d, d %d, s (%d, %d) st %d\n",
        __FUNCTION__, surface_id, (uint64_t)*phys_mem, *surface_format,
        depth, cx, cy, *stride));
    size = abs(*stride) * cy;
    if (!base_mem) {
        DEBUG_PRINT((pdev, 0, "%s: %p: %d: QXLGetSurface failed (%d bytes alloc)\n",
            __FUNCTION__, pdev, surface_id, size));
        return NULL;
    }
    if (!CreateDrawArea(pdev, base_mem, surface_info->bitmap_format, cx, cy, *stride, surface_id)) {
        DEBUG_PRINT((pdev, 0, "%s: %p: CreateDrawArea failed (%d)\n",
            __FUNCTION__, pdev, surface_id, size));
        // TODO: Why did it fail? nothing in the MSDN
        QXLDelSurface(pdev, base_mem, allocation_type);
        return NULL;
    }
    return base_mem;
}

static void SendSurfaceCreateCommand(PDev *pdev, UINT32 surface_id, SIZEL size,
                                     UINT32 surface_format, INT32 stride, QXLPHYSICAL phys_mem,
                                     int keep_data)
{
    QXLSurfaceCmd *surface;

    surface = SurfaceCmd(pdev, QXL_SURFACE_CMD_CREATE, surface_id);
    if (keep_data) {
        surface->flags |= QXL_SURF_FLAG_KEEP_DATA;
    }
    surface->u.surface_create.format = surface_format;
    surface->u.surface_create.width = size.cx;
    surface->u.surface_create.height = size.cy;
    surface->u.surface_create.stride = stride;
    surface->u.surface_create.data = phys_mem;
    PushSurfaceCmd(pdev, surface);
}

HBITMAP CreateDeviceBitmap(PDev *pdev, SIZEL size, ULONG format, QXLPHYSICAL *phys_mem,
                           UINT8 **base_mem, UINT32 surface_id, UINT8 allocation_type)
{
    UINT32 surface_format, depth;
    HBITMAP hbitmap;
    INT32 stride;
    SurfaceInfo *surface_info;

    DEBUG_PRINT((pdev, 9, "%s: %p: %d, (%dx%d), %d\n", __FUNCTION__, pdev, surface_id,
                size.cx, size.cy, format));
    surface_info = GetSurfaceInfo(pdev, surface_id);

    if (!(hbitmap = EngCreateDeviceBitmap((DHSURF)surface_info, size, format))) {
        DEBUG_PRINT((pdev, 0, "%s: EngCreateDeviceBitmap failed, pdev 0x%lx, surface_id=%d\n",
                    __FUNCTION__, pdev, surface_id));
        goto out_error1;
    }

    if (!EngAssociateSurface((HSURF)hbitmap, pdev->eng, QXL_SURFACE_HOOKS)) {
        DEBUG_PRINT((pdev, 0, "%s: EngAssociateSurface failed\n", __FUNCTION__));
        goto out_error2;
    }
    surface_info->u.pdev = pdev;
    surface_info->hbitmap = hbitmap;
    surface_info->copy = NULL;
    surface_info->size = size;
    surface_info->bitmap_format = format;
    if ((*base_mem = CreateSurfaceHelper(pdev, surface_id, size.cx, size.cy, format,
                                         allocation_type, &stride, &surface_format,
                                         phys_mem)) == NULL) {
        DEBUG_PRINT((pdev, 0, "%s: failed, pdev 0x%lx, surface_id=%d\n",
                    __FUNCTION__, pdev, surface_id));
        goto out_error2;
    }
    surface_info->stride = stride;
    if (allocation_type != DEVICE_BITMAP_ALLOCATION_TYPE_SURF0) {
        SendSurfaceCreateCommand(pdev, surface_id, size, surface_format, -stride, *phys_mem, 0);
    }

    return hbitmap;
out_error2:
    EngDeleteSurface((HSURF)hbitmap);
out_error1:
    return 0;
}

VOID DeleteDeviceBitmap(PDev *pdev, UINT32 surface_id, UINT8 allocation_type)
{
    DrawArea *drawarea;

    drawarea = &GetSurfaceInfo(pdev,surface_id)->draw_area;

    FreeDrawArea(drawarea);

    if (allocation_type != DEVICE_BITMAP_ALLOCATION_TYPE_SURF0 &&
        pdev->surfaces_info[surface_id].draw_area.base_mem != NULL) {

        if (allocation_type == DEVICE_BITMAP_ALLOCATION_TYPE_RAM) {
            /* server side this surface is already destroyed, just free it here */
            ASSERT(pdev, pdev->surfaces_info[surface_id].draw_area.base_mem ==
                         pdev->surfaces_info[surface_id].copy);
            QXLDelSurface(pdev,
                          pdev->surfaces_info[surface_id].draw_area.base_mem,
                          allocation_type);
            FreeSurfaceInfo(pdev, surface_id);
        } else {
            QXLSurfaceCmd *surface_cmd;
            surface_cmd = SurfaceCmd(pdev, QXL_SURFACE_CMD_DESTROY, surface_id);
            QXLGetDelSurface(pdev, surface_cmd, surface_id, allocation_type);
            PushSurfaceCmd(pdev, surface_cmd);
        }
    }
}

static void CleanupSurfaceInfo(PDev *pdev, UINT32 surface_id, UINT8 allocation_type)
{
    SurfaceInfo *surface_info = GetSurfaceInfo(pdev, surface_id);

    FreeDrawArea(&surface_info->draw_area);
    if (surface_info->draw_area.base_mem != NULL) {
        QXLDelSurface(pdev, surface_info->draw_area.base_mem, allocation_type);
    }
}

BOOL MoveSurfaceToVideoRam(PDev *pdev, UINT32 surface_id)
{
    QXLSurfaceCmd *surface;
    UINT32 surface_format;
    UINT32 depth;
    int count_used = 0;
    int size;
    INT32 stride = 0;
    QXLPHYSICAL phys_mem;
    SurfaceInfo *surface_info = GetSurfaceInfo(pdev, surface_id);
    UINT32 cx = surface_info->size.cx;
    UINT32 cy = surface_info->size.cy;
    UINT8 *base_mem;

    DEBUG_PRINT((pdev, 3, "%s: %d\n", __FUNCTION__, surface_id));
    if ((base_mem = CreateSurfaceHelper(pdev, surface_id, cx, cy, surface_info->bitmap_format,
                                        DEVICE_BITMAP_ALLOCATION_TYPE_VRAM,
                                        &stride, &surface_format, &phys_mem)) == NULL) {
        DEBUG_PRINT((pdev, 0, "%s: %p: %d: failed\n", __FUNCTION__, pdev, surface_id));
        return FALSE;
    }
    size = abs(stride) * cy;
    if (!EngModifySurface((HSURF)surface_info->hbitmap, pdev->eng, QXL_SURFACE_HOOKS,
        MS_NOTSYSTEMMEMORY, (DHSURF)surface_info, NULL, 0, NULL)) {
        DEBUG_PRINT((pdev, 0, "%s: %p: %d: EngModifySurface failed\n",
            __FUNCTION__, pdev, surface_id));
        CleanupSurfaceInfo(pdev, surface_id, DEVICE_BITMAP_ALLOCATION_TYPE_VRAM);
        return FALSE;
    }
    DEBUG_PRINT((pdev, 3, "%s: stride = %d, phys_mem = %0lX, base_mem = %p\n",
        __FUNCTION__, -stride, (uint64_t)phys_mem, base_mem));
    DEBUG_PRINT((pdev, 3, "%s: copy %d bytes to %d\n", __FUNCTION__, size, surface_id));
    // Everything allocated, nothing can fail (API wise) from this point
    RtlCopyMemory(base_mem, surface_info->copy, size);
    EngFreeMem(surface_info->copy);
    surface_info->copy = NULL;
    SendSurfaceCreateCommand(pdev, surface_id, surface_info->size, surface_format,
                             -stride, phys_mem, 1);
    return TRUE;
}

/* when we return from S3 we need to resend all the surface creation commands.
 * Actually moving the memory vram<->guest is not strictly neccessary since vram
 * is not reset during the suspend, so contents are not lost */
int MoveAllSurfacesToVideoRam(PDev *pdev)
{
    UINT32 surface_id;
    SurfaceInfo *surface_info;

    /* brute force implementation - alternative is to keep an updated used_surfaces list */
    DEBUG_PRINT((pdev, 3, "%s %p\n", __FUNCTION__, pdev));

    for (surface_id = 1 ; surface_id < pdev->n_surfaces ; ++surface_id) {
        surface_info = GetSurfaceInfo(pdev, surface_id);
        if (!surface_info->draw_area.base_mem) {
            continue;
        }
        if (surface_info->u.pdev != pdev) {
            DEBUG_PRINT((pdev, 3, "%s: %p: not our pdev (%p)\n", __FUNCTION__, pdev,
                         surface_info->u.pdev));
            continue;
        }
        if (surface_info->draw_area.surf_obj) {
            DEBUG_PRINT((pdev, 3, "%s: surface_id = %d, surf_obj not empty\n", __FUNCTION__,
                         surface_id));
            continue;
        }
        if (surface_info->copy == NULL) {
            DEBUG_PRINT((pdev, 3, "%s: %p: %d: no copy buffer, ignored\n", __FUNCTION__,
                         pdev, surface_id));
            continue;
        }
        if (!MoveSurfaceToVideoRam(pdev, surface_id)) {
            /* Some of the surfaces have not been moved to video ram.
             * they will remain managed by GDI. */
            DEBUG_PRINT((pdev, 0, "%s: %p: %d: failed moving to vram\n", __FUNCTION__,
                         pdev, surface_id));
        }
    }
    return TRUE;
}

/* to_surface_id is exclusive */
static void SendSurfaceRangeCreateCommand(PDev *pdev, UINT32 from_surface_id, UINT32 to_surface_id)
{
    UINT32 surface_id;

    ASSERT(pdev, from_surface_id < to_surface_id);
    ASSERT(pdev, to_surface_id <= pdev->n_surfaces);

    for (surface_id = from_surface_id; surface_id < to_surface_id; surface_id++) {
        SurfaceInfo *surface_info;
        SURFOBJ *surf_obj;
        QXLPHYSICAL phys_mem;
        UINT32 surface_format;
        UINT32 depth;

        surface_info = GetSurfaceInfo(pdev, surface_id);
        if (!surface_info->draw_area.base_mem) {
            continue;
        }

        surf_obj = surface_info->draw_area.surf_obj;

        if (!surf_obj) {
            continue;
        }

        phys_mem = SurfaceToPhysical(pdev, surface_info->draw_area.base_mem);
        BitmapFormatToDepthAndSurfaceFormat(surface_info->bitmap_format, &depth, &surface_format);

        SendSurfaceCreateCommand(pdev, surface_id, surf_obj->sizlBitmap,
                                 surface_format, -surface_info->stride, phys_mem,
                                 /* the surface is still there, tell server not to erase */
                                 1);
    }
}

BOOL MoveAllSurfacesToRam(PDev *pdev)
{
    UINT32 surface_id;
    SurfaceInfo *surface_info;
    SURFOBJ *surf_obj;
    UINT8 *copy;
    UINT8 *line0;
    int size;
    QXLPHYSICAL phys_mem;

    for (surface_id = 1 ; surface_id < pdev->n_surfaces ; ++surface_id) {
        surface_info = GetSurfaceInfo(pdev, surface_id);
        if (!surface_info->draw_area.base_mem) {
            continue;
        }
        surf_obj = surface_info->draw_area.surf_obj;
        if (!surf_obj) {
            DEBUG_PRINT((pdev, 3, "%s: %d: no surfobj, not copying\n", __FUNCTION__, surface_id));
            continue;
        }
        size = surf_obj->sizlBitmap.cy * abs(surf_obj->lDelta);
        copy = EngAllocMem(0, size, ALLOC_TAG);
        DEBUG_PRINT((pdev, 3, "%s: %d: copying #%d to %p (%d)\n", __FUNCTION__, surface_id, size,
            copy, surf_obj->lDelta));
        RtlCopyMemory(copy, surface_info->draw_area.base_mem, size);
        surface_info->copy = copy;
        line0 = surf_obj->lDelta > 0 ? copy : copy + abs(surf_obj->lDelta) *
                (surf_obj->sizlBitmap.cy - 1);
        if (!EngModifySurface((HSURF)surface_info->hbitmap,
                      pdev->eng,
                      0, /* from the example: used to monitor memory HOOK_COPYBITS | HOOK_BITBLT, */
                      0,                    /* It's system-memory */
                      (DHSURF)surface_info,
                      line0,
                      surf_obj->lDelta,
                      NULL)) {
            /* Send a create messsage for this surface - we previously did a destroy all. */
            EngFreeMem(surface_info->copy);
            surface_info->copy = NULL;
            DEBUG_PRINT((pdev, 0, "%s: %d: EngModifySurface failed, sending create for %d-%d\n",
                         __FUNCTION__, surface_id, surface_id, pdev->n_surfaces - 1));
            SendSurfaceRangeCreateCommand(pdev, surface_id, pdev->n_surfaces);
            return FALSE;
        }
        QXLDelSurface(pdev, surface_info->draw_area.base_mem, DEVICE_BITMAP_ALLOCATION_TYPE_VRAM);
        surface_info->draw_area.base_mem = copy;
        FreeDrawArea(&surface_info->draw_area);
    }
    return TRUE;
}
