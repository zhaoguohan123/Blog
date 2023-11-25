#ifndef SURFACE_H
#define SURFACE_H

#include "qxldd.h"

/* Hooks supported by our surfaces. */
#ifdef CALL_TEST
#define QXL_SURFACE_HOOKS_CALL_TEST \
    (HOOK_PLGBLT | HOOK_FILLPATH | HOOK_STROKEANDFILLPATH | HOOK_LINETO |  \
    HOOK_GRADIENTFILL)
#else
#define QXL_SURFACE_HOOKS_CALL_TEST (0)
#endif

#define QXL_SURFACE_HOOKS \
    (HOOK_SYNCHRONIZE | HOOK_COPYBITS |                                 \
    HOOK_BITBLT | HOOK_TEXTOUT | HOOK_STROKEPATH | HOOK_STRETCHBLT |    \
    HOOK_STRETCHBLTROP | HOOK_TRANSPARENTBLT | HOOK_ALPHABLEND | QXL_SURFACE_HOOKS_CALL_TEST)


static _inline UINT32 GetSurfaceIdFromInfo(SurfaceInfo *info)
{
  PDev *pdev;

  pdev = info->u.pdev;
  if (info == &pdev->surface0_info) {
    return 0;
  }
  return (UINT32)(info - pdev->surfaces_info);
}

static _inline SurfaceInfo *GetSurfaceInfo(PDev *pdev, UINT32 id)
{
  if (id == 0) {
    return &pdev->surface0_info;
  }
  return &pdev->surfaces_info[id];
}

static _inline UINT32 GetSurfaceId(SURFOBJ *surf)
{
    SurfaceInfo *surface;

    if (!surf || !surf->dhsurf) {
        return (UINT32)-1;
    }
    surface = (SurfaceInfo *)surf->dhsurf;
    return GetSurfaceIdFromInfo(surface);
}

static _inline void FreeSurfaceInfo(PDev *pdev, UINT32 surface_id)
{
    SurfaceInfo *surface;

    if (surface_id == 0) {
        return;
    }

    DEBUG_PRINT((pdev, 9, "%s: %p: %d\n", __FUNCTION__, pdev, surface_id));
    surface = &pdev->surfaces_info[surface_id];
    if (surface->draw_area.base_mem == NULL) {
        DEBUG_PRINT((pdev, 9, "%s: %p: %d: double free. safely ignored\n", __FUNCTION__,
                     pdev, surface_id));
        return;
    }
    surface->draw_area.base_mem = NULL; /* Mark as not used */
    surface->u.next_free = pdev->free_surfaces;
    pdev->free_surfaces = surface;
}

static UINT32 GetFreeSurface(PDev *pdev)
{
    UINT32 x, id;
    SurfaceInfo *surface;

    ASSERT(pdev, pdev->enabled);
    surface = pdev->free_surfaces;
    if (surface == NULL) {
        id = 0;
    } else {
      pdev->free_surfaces = surface->u.next_free;

      id = (UINT32)(surface - pdev->surfaces_info);
    }

    return id;
}

enum {
    DEVICE_BITMAP_ALLOCATION_TYPE_SURF0,
    DEVICE_BITMAP_ALLOCATION_TYPE_DEVRAM,
    DEVICE_BITMAP_ALLOCATION_TYPE_VRAM,
    DEVICE_BITMAP_ALLOCATION_TYPE_RAM,
};

HBITMAP CreateDeviceBitmap(PDev *pdev, SIZEL size, ULONG format, QXLPHYSICAL *phys_mem,
                           UINT8 **base_mem, UINT32 surface_id, UINT8 allocation_type);
VOID DeleteDeviceBitmap(PDev *pdev, UINT32 surface_id, UINT8 allocation_type);

int MoveAllSurfacesToVideoRam(PDev *pdev);
BOOL MoveAllSurfacesToRam(PDev *pdev);

#endif
