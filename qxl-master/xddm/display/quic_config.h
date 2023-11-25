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

#ifndef __QUIC_CONFIG_H
#define __QUIC_CONFIG_H

#include <spice/types.h>

#ifdef __GNUC__

#include <string.h>

#define INLINE inline

#define MEMCLEAR(ptr, size) memset(ptr, 0, size)

#else

#ifdef QXLDD
#include <windef.h>
#include "os_dep.h"
#define INLINE _inline
#define MEMCLEAR(ptr, size) RtlZeroMemory(ptr, size)
#else
#include <stddef.h>
#include <string.h>

#define INLINE inline
#define MEMCLEAR(ptr, size) memset(ptr, 0, size)
#endif


#endif

#endif

