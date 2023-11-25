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

#ifndef WDM_HELPER_H
#define WDM_HELPER_H

#include "os_dep.h"

typedef ULONG (*PQXLWaitForEvent)(PVOID,PLARGE_INTEGER);

LONG QXLInitializeEvent(PVOID * pEvent);
void QXLSetEvent(PVOID pEvent);
void QXLDeleteEvent(PVOID pEvent);
ULONG QXLWaitForEvent(PVOID pEvent,PLARGE_INTEGER Timeout);

#define VideoPortDeleteEvent(dev,pEvent) QXLDeleteEvent(pEvent)
#define VideoPortCreateEvent(dev,flag,reserved,ppEvent) QXLInitializeEvent(ppEvent)
#define VideoPortSetEvent(dev,pEvent) QXLSetEvent(pEvent)

#endif
