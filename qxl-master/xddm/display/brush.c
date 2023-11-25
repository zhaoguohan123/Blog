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

#include "os_dep.h"
#include "qxldd.h"
#include "utils.h"
#include "res.h"

typedef struct InternalBrush {
    ULONG format;
    SIZEL size;
    UINT32 stride;
    UINT32 key;
    UINT8 data[0];
} InternalBrush;

#define COPY1BPP(bits)                                                                  \
static _inline void realizeBrush##bits##Copy1bpp(PDev *pdev, InternalBrush *internal,   \
                                                 UINT8 *src, LONG src_stride,           \
                                                 XLATEOBJ *color_trans)                 \
{                                                                                       \
    UINT8 *end = src + src_stride * internal->size.cy;                                  \
    UINT8 *dest = internal->data;                                                       \
                                                                                        \
    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));                                       \
    ASSERT(pdev, color_trans && color_trans->flXlate & XO_TABLE);                       \
    for (; src < end; src += src_stride, dest += internal->stride) {                    \
        UINT##bits *now = (UINT##bits *)dest;                                           \
        int i;                                                                          \
                                                                                        \
        for (i = 0; i < internal->size.cx; i++) {                                       \
            *(now++) = (UINT##bits)color_trans->pulXlate[test_bit_be(src, i)];          \
        }                                                                               \
    }                                                                                   \
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));                                 \
}

#define COPY4BPP(bits)                                                                  \
static _inline void realizeBrush##bits##Copy4bpp(PDev *pdev, InternalBrush *internal,   \
                                                 UINT8 *src, LONG src_stride,           \
                                                 XLATEOBJ *color_trans)                 \
{                                                                                       \
    UINT8 *dest = internal->data;                                                       \
    UINT8 *end = dest + internal->stride * internal->size.cy;                           \
                                                                                        \
    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));                                       \
    ASSERT(pdev, color_trans && color_trans->flXlate & XO_TABLE);                       \
    for (; dest < end; dest += internal->stride, src += src_stride) {                   \
        UINT8 *src_line = src;                                                          \
        UINT8 *src_line_end = src_line + (internal->size.cx >> 1);                      \
        UINT##bits *dest_line = (UINT##bits *)dest;                                     \
                                                                                        \
        for (; src_line < src_line_end; src_line++) {                                   \
            ASSERT(pdev, (*src_line & 0x0fU) < color_trans->cEntries);                  \
            ASSERT(pdev, ((*src_line >> 4) & 0x0fU) < color_trans->cEntries);           \
            *(dest_line++) = (UINT##bits)color_trans->pulXlate[(*src_line >> 4) & 0x0f];\
            *(dest_line++) = (UINT##bits)color_trans->pulXlate[*src_line & 0x0f];       \
        }                                                                               \
        if (internal->size.cx & 1) {                                                    \
            *(dest_line) = (UINT##bits)color_trans->pulXlate[(*src_line >> 4) & 0x0f];  \
        }                                                                               \
    }                                                                                   \
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));                                 \
}

#define COPY8BPP(bits)                                                                  \
static _inline void realizeBrush##bits##Copy8bpp(PDev *pdev, InternalBrush *internal,   \
                                                 UINT8 *src, LONG src_stride,           \
                                                 XLATEOBJ *color_trans)                 \
{                                                                                       \
    UINT8 *dest = internal->data;                                                       \
    UINT8 *end = dest + internal->stride * internal->size.cy;                           \
                                                                                        \
    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));                                       \
    ASSERT(pdev, color_trans && color_trans->flXlate & XO_TABLE);                       \
    for (; dest < end; dest += internal->stride, src += src_stride) {                   \
        UINT8 *src_line = src;                                                          \
        UINT8 *src_line_end = src_line + internal->size.cx;                             \
        UINT##bits *dest_line = (UINT##bits *)dest;                                     \
                                                                                        \
        for (; src_line < src_line_end; ++dest_line, src_line++) {                      \
            ASSERT(pdev, *src_line < color_trans->cEntries);                            \
            *dest_line = (UINT##bits)color_trans->pulXlate[*src_line];                  \
        }                                                                               \
    }                                                                                   \
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));                                 \
}

COPY1BPP(16);
COPY1BPP(32);
COPY4BPP(16);
COPY4BPP(32);
COPY8BPP(16);
COPY8BPP(32);

static _inline void realizeBrush32Copy16bpp(PDev *pdev, InternalBrush *internal, UINT8 *src,
                                          LONG src_stride)
{
    UINT8 *dest = internal->data;
    UINT8 *end = dest + internal->stride * internal->size.cy;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    for (; dest < end; dest += internal->stride, src += src_stride) {
        UINT32 *dest_line = (UINT32 *)dest;
        UINT32 *dest_line_end = dest_line + internal->size.cx;
        UINT16 *src_line = (UINT16 *)src;

        for (; dest_line < dest_line_end; ++dest_line, src_line++) {
            *dest_line = _16bppTo32bpp(*src_line);
        }
    }
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
}

static _inline void realizeBrush32Copy24bpp(PDev *pdev, InternalBrush *internal, UINT8 *src,
                                          LONG src_stride)
{
    UINT8 *dest = internal->data;
    UINT8 *end = dest + internal->stride * internal->size.cy;
    int line_size = internal->size.cx << 2;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    for (; dest < end; dest += internal->stride, src += src_stride) {
        UINT8* dest_line = dest;
        UINT8* dest_line_end = dest_line + line_size;
        UINT8* src_line = src;

        while (dest_line < dest_line_end) {
            *(dest_line++) = *(src_line++);
            *(dest_line++) = *(src_line++);
            *(dest_line++) = *(src_line++);
            *(dest_line++) = 0;
        }
    }
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
}

static _inline void realizeBrush32Copy32bpp(PDev *pdev, InternalBrush *internal, UINT8 *src,
                                          LONG src_stride)
{
    UINT8 *now = internal->data;
    UINT8 *end = now + internal->stride * internal->size.cy;
    int line_size = internal->size.cx << 2;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    for (; now < end; now += internal->stride, src += src_stride) {
        RtlCopyMemory(now, src, line_size);
    }
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
}

static _inline void realizeBrush16Copy16bpp(PDev *pdev, InternalBrush *internal, UINT8 *src,
                                          LONG src_stride)
{
    UINT8 *dest = internal->data;
    UINT8 *end = dest + internal->stride * internal->size.cy;
    int line_size = internal->size.cx << 1;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    for (; dest < end; dest += internal->stride, src += src_stride) {
        RtlCopyMemory(dest, src, line_size);
    }
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
}

static _inline void realizeBrush16Copy24bpp(PDev *pdev, InternalBrush *internal, UINT8 *src,
                                          LONG src_stride)
{
    UINT8 *dest = internal->data;
    UINT8 *end = dest + internal->stride * internal->size.cy;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    for (; dest < end; dest += internal->stride, src += src_stride) {
        UINT16 *dest_line = (UINT16 *)dest;
        UINT16 *dest_line_end = dest_line + internal->size.cx;
        UINT8 *src_line = src;

        while (dest_line < dest_line_end) {
            *(dest_line++) = ((UINT16)*(src_line++) >> 3) |
                             (((UINT16)*(src_line++) & 0xf8) << 2) |
                             (((UINT16)*(src_line++) & 0xf8) << 7);
        }
    }
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
}

UINT16 _32bppTo16bpp(UINT32 color)
{
    return ((color & 0xf8) >> 3) | ((color & 0xf800) >> 6) | ((color & 0xf80000) >> 9);
}

static _inline void realizeBrush16Copy32bpp(PDev *pdev, InternalBrush *internal, UINT8 *src,
                                          LONG src_stride)
{
    UINT8 *now = internal->data;
    UINT8 *end = now + internal->stride * internal->size.cy;

    DEBUG_PRINT((pdev, 6, "%s\n", __FUNCTION__));
    for (; now < end; now += internal->stride, src += src_stride) {
        UINT16 *dest_line = (UINT16 *)now;
        UINT16 *dest_line_end = dest_line + internal->size.cx;
        UINT32 *src_line = (UINT32 *)src;

        while (dest_line < dest_line_end) {
            *(dest_line++) = _32bppTo16bpp(*(src_line++));
        }
    }
    DEBUG_PRINT((pdev, 7, "%s: done\n", __FUNCTION__));
}

BOOL APIENTRY DrvRealizeBrush(BRUSHOBJ *brush, SURFOBJ *target, SURFOBJ *pattern, SURFOBJ *mask,
                              XLATEOBJ *color_trans, ULONG hatch)
{
    PDev *pdev;
    InternalBrush *internal;
    int stride;
    int size;

    if (!(pdev = (PDev *)target->dhpdev)) {
        DEBUG_PRINT((NULL, 0, "%s: err no pdev\n", __FUNCTION__));
        return FALSE;
    }

    PUNT_IF_DISABLED(pdev);

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));

    if (mask) {
        DEBUG_PRINT((pdev, 0, "%s: ignoring mask\n", __FUNCTION__));
    }

    switch (pattern->iBitmapFormat) {
    case BMF_1BPP:
    case BMF_32BPP:
    case BMF_24BPP:
    case BMF_16BPP:
    case BMF_8BPP:
    case BMF_4BPP:
        break;
    default:
        DEBUG_PRINT((pdev, 0, "%s: bad format\n", __FUNCTION__));
        return FALSE;
    }
    stride = (pdev->bitmap_format == BMF_32BPP) ? pattern->sizlBitmap.cx << 2 :
                                                 ALIGN(pattern->sizlBitmap.cx << 1, 4);
    size = stride * pattern->sizlBitmap.cy;
    size += sizeof(InternalBrush);

    if (!(internal = (InternalBrush *)BRUSHOBJ_pvAllocRbrush(brush, size))) {
        DEBUG_PRINT((pdev, 0, "%s: alloc failed\n", __FUNCTION__));
        return FALSE;
    }
    internal->size = pattern->sizlBitmap;
    internal->key = 0;
    internal->stride = stride;
    if ((internal->format = pdev->bitmap_format) == BMF_32BPP) {
        switch (pattern->iBitmapFormat) {
        case BMF_1BPP:
            realizeBrush32Copy1bpp(pdev, internal, pattern->pvScan0, pattern->lDelta, color_trans);
            break;
        case BMF_32BPP:
            realizeBrush32Copy32bpp(pdev, internal, pattern->pvScan0, pattern->lDelta);
            break;
        case BMF_24BPP:
            realizeBrush32Copy24bpp(pdev, internal, pattern->pvScan0, pattern->lDelta);
            break;
        case BMF_16BPP:
            realizeBrush32Copy16bpp(pdev, internal, pattern->pvScan0, pattern->lDelta);
            break;
        case BMF_8BPP:
            realizeBrush32Copy8bpp(pdev, internal, pattern->pvScan0, pattern->lDelta, color_trans);
            break;
        case BMF_4BPP:
            realizeBrush32Copy4bpp(pdev, internal, pattern->pvScan0, pattern->lDelta, color_trans);
            break;
        }
    } else {
        ASSERT(pdev, pdev->bitmap_format == BMF_16BPP);
        switch (pattern->iBitmapFormat) {
        case BMF_1BPP:
            realizeBrush16Copy1bpp(pdev, internal, pattern->pvScan0, pattern->lDelta, color_trans);
            break;
        case BMF_32BPP:
            realizeBrush16Copy32bpp(pdev, internal, pattern->pvScan0, pattern->lDelta);
            break;
        case BMF_24BPP:
            realizeBrush16Copy24bpp(pdev, internal, pattern->pvScan0, pattern->lDelta);
            break;
        case BMF_16BPP:
            realizeBrush16Copy16bpp(pdev, internal, pattern->pvScan0, pattern->lDelta);
            break;
        case BMF_8BPP:
            realizeBrush16Copy8bpp(pdev, internal, pattern->pvScan0, pattern->lDelta, color_trans);
            break;
        case BMF_4BPP:
            realizeBrush16Copy4bpp(pdev, internal, pattern->pvScan0, pattern->lDelta, color_trans);
            break;
        }
    }
    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));
    return TRUE;
}


static _inline BOOL GetPattern(PDev *pdev, QXLDrawable *drawable, QXLPHYSICAL *pattern,
                               InternalBrush *brush, INT32 *surface_dest,
                               QXLRect *surface_rect)
{
    HSURF hsurf;
    SURFOBJ *surf_obj;
    QXLRect area;
    UINT32 key;

    DEBUG_PRINT((pdev, 12, "%s\n", __FUNCTION__));
    if (brush->key && QXLGetBitsFromCache(pdev, drawable, brush->key, pattern)) {
        DEBUG_PRINT((pdev, 13, "%s: from cache\n", __FUNCTION__));
        return TRUE;
    }

    if (!(hsurf = (HSURF)EngCreateBitmap(brush->size, brush->stride, brush->format, BMF_TOPDOWN,
                                         brush->data))) {
        DEBUG_PRINT((pdev, 0, "%s: create bitmap failed\n", __FUNCTION__));
        return FALSE;
    }

    if (!(surf_obj = EngLockSurface(hsurf))) {
        DEBUG_PRINT((pdev, 0, "%s: lock surf failed\n", __FUNCTION__));
        goto error_1;
    }
    area.left = area.top = 0;
    area.right = brush->size.cx;
    area.bottom = brush->size.cy;

    CopyRect(surface_rect, &area);

    if (!QXLGetBitmap(pdev, drawable, pattern, surf_obj, &area, NULL, &key, TRUE, surface_dest)) {
        goto error_2;
    }

    brush->key = key;
    EngUnlockSurface(surf_obj);
    EngDeleteSurface(hsurf);
    DEBUG_PRINT((pdev, 13, "%s: done\n", __FUNCTION__));
    return TRUE;

error_2:
    EngUnlockSurface(surf_obj);
error_1:
    EngDeleteSurface(hsurf);
    return FALSE;
}


BOOL QXLGetBrush(PDev *pdev, QXLDrawable *drawable, QXLBrush *qxl_brush,
                 BRUSHOBJ *brush, POINTL *brush_pos, INT32 *surface_dest,
                 QXLRect *surface_rect)
{
    DEBUG_PRINT((pdev, 9, "%s\n", __FUNCTION__));
    ASSERT(pdev, brush);

    if (brush->iSolidColor == ~0) {
        ASSERT(pdev, brush_pos);

        DEBUG_PRINT((pdev, 11, "%s: pattern\n", __FUNCTION__));
        if (!brush->pvRbrush && !BRUSHOBJ_pvGetRbrush(brush)) {
            DEBUG_PRINT((pdev, 0, "%s: brush realize failed\n", __FUNCTION__));
            return FALSE;
        }
        qxl_brush->type = SPICE_BRUSH_TYPE_PATTERN;
        qxl_brush->u.pattern.pos.x = brush_pos->x;
        qxl_brush->u.pattern.pos.y = brush_pos->y;
        if (!GetPattern(pdev, drawable, &qxl_brush->u.pattern.pat, brush->pvRbrush,
                        surface_dest, surface_rect)) {
            return FALSE;
        }
    } else {
        qxl_brush->type = SPICE_BRUSH_TYPE_SOLID;
        qxl_brush->u.color = brush->iSolidColor;
        DEBUG_PRINT((pdev, 11, "%s: color 0x%x\n", __FUNCTION__, qxl_brush->u.color));
    }
    DEBUG_PRINT((pdev, 10, "%s: done\n", __FUNCTION__));
    return TRUE;
}

