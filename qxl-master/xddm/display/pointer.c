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

/* DDK quote

Calls to the pointer functions are serialized by GDI. This means two different threads in the
driver cannot execute the pointer functions simultaneously.

*/

ULONG APIENTRY DrvSetPointerShape(SURFOBJ *surf, SURFOBJ *mask, SURFOBJ *color_pointer,
                                  XLATEOBJ *color_trans, LONG hot_x, LONG hot_y,
                                  LONG pos_x, LONG pos_y, RECTL *prcl, FLONG flags)
{
    QXLCursorCmd *cursor_cmd;
    PDev *pdev;

    if (!(pdev = (PDev *)surf->dhpdev)) {
        DEBUG_PRINT((NULL, 0, "%s: err no pdev\n", __FUNCTION__));
        return SPS_ERROR;
    }

    PUNT_IF_DISABLED(pdev);

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));

    if (flags & SPS_CHANGE) {
        BOOL ok;
        cursor_cmd = CursorCmd(pdev);
        cursor_cmd->type = QXL_CURSOR_SET;
        if (flags & SPS_ALPHA) {
            if (mask) {
                DEBUG_PRINT((pdev, 0, "%s: SPS_ALPHA and mask \n", __FUNCTION__));
            }
            ASSERT(pdev, color_pointer);
            ok = GetAlphaCursor(pdev, cursor_cmd, hot_x, hot_y, color_pointer);
        } else if (!mask) {
            ok = GetTransparentCursor(pdev, cursor_cmd);
        } else if (color_pointer && color_pointer->iBitmapFormat != BMF_1BPP) {
            ASSERT(pdev, mask);
            ok = GetColorCursor(pdev, cursor_cmd, hot_x, hot_y, color_pointer, mask, color_trans);
        } else {
            ok = GetMonoCursor(pdev, cursor_cmd, hot_x, hot_y, mask);
        }

        if (!ok) {
            DEBUG_PRINT((pdev, 0, "%s: get cursor failed\n", __FUNCTION__));
            ReleaseOutput(pdev, cursor_cmd->release_info.id);
            return SPS_ERROR;
        }
        if (pos_x < 0) {
            cursor_cmd->u.set.visible = FALSE;
            cursor_cmd->u.set.position.x = cursor_cmd->u.set.position.y = 0;
        } else {
            cursor_cmd->u.set.visible = TRUE;
            cursor_cmd->u.set.position.x = (INT16)pos_x;
            cursor_cmd->u.set.position.y = (INT16)pos_y;
        }

        PushCursorCmd(pdev, cursor_cmd);

    } else {
        cursor_cmd = CursorCmd(pdev);
        if (pos_x < 0) {
#ifdef DBG
            DEBUG_PRINT((pdev, 0, "%s: no SPS_CHANGE and pos_x < 0\n", __FUNCTION__));
#endif
            cursor_cmd->type = QXL_CURSOR_HIDE;
        } else {
#ifdef DBG
            DEBUG_PRINT((pdev, 0, "%s: no SPS_CHANGE\n", __FUNCTION__));
#endif
            cursor_cmd->type = QXL_CURSOR_MOVE;
            cursor_cmd->u.position.x = (INT16)pos_x;
            cursor_cmd->u.position.y = (INT16)pos_y;
        }
        PushCursorCmd(pdev, cursor_cmd);
    }
#if (WINVER >= 0x0501)
    if ((flags & (SPS_LENGTHMASK | SPS_FREQMASK)) != pdev->cursor_trail){
        pdev->cursor_trail = (flags & (SPS_LENGTHMASK | SPS_FREQMASK));
        cursor_cmd = CursorCmd(pdev);
        cursor_cmd->type = QXL_CURSOR_TRAIL;
        cursor_cmd->u.trail.length = (UINT16)((flags & SPS_LENGTHMASK) >> 8);
        cursor_cmd->u.trail.frequency = (UINT16)((flags & SPS_FREQMASK) >> 12);
        PushCursorCmd(pdev, cursor_cmd);
    }
#endif

    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));
    return SPS_ACCEPT_NOEXCLUDE;
}

VOID APIENTRY DrvMovePointer(SURFOBJ *surf, LONG pos_x, LONG pos_y, RECTL *area)
{
    QXLCursorCmd *cursor_cmd;
    PDev *pdev;

    if (!(pdev = (PDev *)surf->dhpdev)) {
        DEBUG_PRINT((NULL, 0, "%s: err no pdev\n", __FUNCTION__));
        return;
    }

    DEBUG_PRINT((pdev, 3, "%s\n", __FUNCTION__));

    if (pos_y < 0 && pos_x >= 0) {
        DEBUG_PRINT((pdev, 0, "%s: unexpected negative y pos\n", __FUNCTION__));
        return;
    }

    if (!pdev->enabled) {
        DEBUG_PRINT((pdev, 3, "%s: ignoring when device is disabled\n",
                    __FUNCTION__));
        return;
    }

    cursor_cmd = CursorCmd(pdev);
    if (pos_x < 0) {
        cursor_cmd->type = QXL_CURSOR_HIDE;
    } else {
        cursor_cmd->type = QXL_CURSOR_MOVE;
        cursor_cmd->u.position.x = (INT16)pos_x;
        cursor_cmd->u.position.y = (INT16)pos_y;
    }
    PushCursorCmd(pdev, cursor_cmd);

    DEBUG_PRINT((pdev, 4, "%s: done\n", __FUNCTION__));
}

