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

#ifndef _H_ROP
#define _H_ROP

#define ROP3_DEST (1 << 0)
#define ROP3_SRC (1 << 1)
#define ROP3_BRUSH (1 << 2)
#define ROP3_ALL (ROP3_DEST | ROP3_SRC | ROP3_BRUSH)

typedef struct ROP3Info {
    UINT8 effect;
    UINT8 flags;
    UINT32 method_type;
    UINT16 method_data;
} ROP3Info;

extern ROP3Info rops2[];

BOOL BitBltFromDev(PDev *pdev, SURFOBJ *src, SURFOBJ *dest, SURFOBJ *mask, CLIPOBJ *clip,
                   XLATEOBJ *color_trans, RECTL *dest_rect, POINTL src_pos,
                   POINTL *mask_pos, BRUSHOBJ *brush, POINTL *brush_pos, ROP4 rop4);
#endif
