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

#ifndef _H_UTILS
#define _H_UTILS
#include <spice/macros.h>
#define ALIGN(a, b) (((a) + ((b) - 1)) & ~((b) - 1))


#define OFFSETOF(type, member) ((UINT64)&((type *)0)->member)
#define CONTAINEROF(ptr, type, member) \
    ((type *) ((UINT8 *)(ptr) - OFFSETOF(type, member)))

static __inline BOOL IsEmptyRect(RECTL *r)
{
    return r->left >= r->right || r->top >= r->bottom;
}

static __inline void SectRect(RECTL *r1, RECTL *r2, RECTL *dest)
{
    dest->top = MAX(r1->top, r2->top);
    dest->bottom = MAX(MIN(r1->bottom, r2->bottom), dest->top);

    dest->left = MAX(r1->left, r2->left);
    dest->right = MAX(MIN(r1->right, r2->right), dest->left);
}

static _inline LONG RectSize(RECTL *rect)
{
    return (rect->right - rect->left) * (rect->bottom - rect->top);
}

#define CopyRectPoint(dest, src, width, height) \
    (dest)->left = (src)->x; \
    (dest)->right = (src)->x + width; \
    (dest)->top = (src)->y; \
    (dest)->bottom = (src)->y + height; 

#define SameRect(r1, r2) ((r1)->left == (r2)->left && (r1)->right == (r2)->right && \
                          (r1)->top == (r2)->top && (r1)->bottom == (r2)->bottom)

#define CopyRect(dest, src) \
    (dest)->top = (src)->top; \
    (dest)->left = (src)->left; \
    (dest)->bottom = (src)->bottom; \
    (dest)->right = (src)->right;

#define CopyPoint(dest, src) \
    (dest)->x = (src)->x; \
    (dest)->y = (src)->y;

static __inline void FXToRect(RECTL *dest, RECTFX *src)
{
    dest->left = src->xLeft >> 4;
    dest->top = src->yTop >> 4;
    dest->right = ALIGN(src->xRight, 16) >> 4;
    dest->bottom = ALIGN(src->yBottom, 16) >> 4;

}

static _inline int test_bit(void* addr, int bit)
{
    return !!(((UINT32 *)addr)[bit >> 5] & (1 << (bit & 0x1f)));
}

static _inline int test_bit_be(void* addr, int bit)
{
    return !!(((UINT8 *)addr)[bit >> 3] & (0x80 >> (bit & 0x07)));
}

static _inline BOOL PrepareBrush(BRUSHOBJ *brush)
{
    if (!brush || brush->iSolidColor != ~0 || brush->pvRbrush) {
        return TRUE;
    }
    return BRUSHOBJ_pvGetRbrush(brush) != NULL;
}

static _inline BOOL IsCacheableSurf(SURFOBJ *surf, XLATEOBJ *color_trans)
{
    return surf->iUniq && !(surf->fjBitmap & BMF_DONTCACHE) &&
                                                               (!color_trans || color_trans->iUniq);
}

static _inline UINT32 _16bppTo32bpp(UINT32 color)
{
    UINT32 ret;

    ret = ((color & 0x001f) << 3) | ((color & 0x001c) >> 2);
    ret |= ((color & 0x03e0) << 6) | ((color & 0x0380) << 1);
    ret |= ((color & 0x7c00) << 9) | ((color & 0x7000) << 4);

    return ret;
}

static _inline BOOL IsUniqueSurf(SURFOBJ *surf, XLATEOBJ *color_trans)
{
    int pallette = color_trans && (color_trans->flXlate & XO_TABLE);
    return surf->iUniq && (surf->fjBitmap & BMF_DONTCACHE) && (!pallette || color_trans->iUniq);
}

#endif

