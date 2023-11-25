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

#if (WINVER < 0x0501)
#include <ntddk.h>
#include "wdmhelper.h"

void QXLDeleteEvent(PVOID pEvent)
{
    if(pEvent) {
        ExFreePool(pEvent);
    }
}

LONG QXLInitializeEvent(PVOID * pEvent)
{
    if(pEvent) {
        *pEvent = ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT), '_lxq');

        if(*pEvent) {
            KeInitializeEvent((PRKEVENT)*pEvent, SynchronizationEvent, FALSE);
        }
    }

    return 0;
}

void QXLSetEvent(PVOID pEvent)
{
    //Pririty boost can be switched to IO_NO_INCREMENT
    if(pEvent) {
        KeSetEvent((PRKEVENT)pEvent, IO_VIDEO_INCREMENT, FALSE);
    }
}

ULONG QXLWaitForEvent(PVOID pEvent, PLARGE_INTEGER Timeout)
{
    if(pEvent) {
        return NT_SUCCESS(KeWaitForSingleObject(pEvent, Executive, KernelMode, TRUE, Timeout));
    }

    return FALSE;
}

#endif

