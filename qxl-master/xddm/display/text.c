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
#include "rop.h"
#include "surface.h"

BOOL APIENTRY DrvTextOut(SURFOBJ *surf, STROBJ *str, FONTOBJ *font, CLIPOBJ *clip,
                         RECTL *ignored, RECTL *opaque_rect,
                         BRUSHOBJ *fore_brush, BRUSHOBJ *back_brash,
                         POINTL *brushs_origin, MIX mix)
{
    QXLDrawable *drawable;
    ROP3Info *fore_rop;
    ROP3Info *back_rop;
    PDev* pdev;
    RECTL area;
    UINT32 surface_id;

    if (!(pdev = (PDev *)surf->dhpdev)) {
        DEBUG_PRINT((NULL, 0, "%s: err no pdev\n", __FUNCTION__));
        return FALSE;
    }

    PUNT_IF_DISABLED(pdev);

    surface_id = GetSurfaceId(surf);

    CountCall(pdev, CALL_COUNTER_TEXT_OUT);

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));
    ASSERT(pdev, opaque_rect == NULL ||
           (opaque_rect->left < opaque_rect->right && opaque_rect->top < opaque_rect->bottom));
    ASSERT(pdev, surf && str && font && clip);

    if (opaque_rect) {
        CopyRect(&area, opaque_rect);
    } else {
        CopyRect(&area, &str->rclBkGround);
    }

    if (clip) {
        if (clip->iDComplexity == DC_TRIVIAL) {
            clip = NULL;
        } else {
            SectRect(&clip->rclBounds, &area, &area);
            if (IsEmptyRect(&area)) {
                DEBUG_PRINT((pdev, 1, "%s: empty rect after clip\n", __FUNCTION__));
                return TRUE;
            }
        }
    }

    if (!(drawable = Drawable(pdev, QXL_DRAW_TEXT, &area, clip, surface_id))) {
        return FALSE;
    }

    if (opaque_rect) {
        ASSERT(pdev, back_brash && brushs_origin);
        if (!QXLGetBrush(pdev, drawable, &drawable->u.text.back_brush, back_brash, brushs_origin,
                         &drawable->surfaces_dest[0], &drawable->surfaces_rects[0])) {
            goto error;
        }
        CopyRect(&drawable->u.text.back_area, &area);
        drawable->u.text.back_mode = SPICE_ROPD_OP_PUT;
        drawable->effect = QXL_EFFECT_OPAQUE;
    } else {
        drawable->u.text.back_brush.type = SPICE_BRUSH_TYPE_NONE;
        RtlZeroMemory(&drawable->u.text.back_area, sizeof(drawable->u.text.back_area));
        drawable->u.text.back_mode = 0;
        drawable->effect = QXL_EFFECT_BLEND;
    }

    fore_rop = &rops2[(mix - 1) & 0x0f];
    back_rop = &rops2[((mix >> 8) - 1) & 0x0f];

    if (!((fore_rop->flags | back_rop->flags) & ROP3_BRUSH)) {
        drawable->u.stroke.brush.type = SPICE_BRUSH_TYPE_NONE;
    } else if (!QXLGetBrush(pdev, drawable, &drawable->u.text.fore_brush, fore_brush,
                            brushs_origin, &drawable->surfaces_dest[1],
                            &drawable->surfaces_rects[1])) {
        DEBUG_PRINT((pdev, 0, "%s: get brush failed\n", __FUNCTION__));
        goto error;
    }

    if (fore_rop->method_data != back_rop->method_data && back_rop->method_data) {
        DEBUG_PRINT((pdev, 0, "%s: ignoring back rop, fore %u back %u\n",
                     __FUNCTION__,
                     (UINT32)fore_rop->method_data,
                     (UINT32)back_rop->method_data));
    }
    drawable->u.text.fore_mode = fore_rop->method_data;

    if (!QXLGetStr(pdev, drawable, &drawable->u.text.str, font, str)) {
        DEBUG_PRINT((pdev, 0, "%s: get str failed\n", __FUNCTION__));
        goto error;
    }

    PushDrawable(pdev, drawable);
    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));
    return TRUE;

error:
    ReleaseOutput(pdev, drawable->release_info.id);
    DEBUG_PRINT((pdev, 4, "%s: error\n", __FUNCTION__));
    return FALSE;
}
