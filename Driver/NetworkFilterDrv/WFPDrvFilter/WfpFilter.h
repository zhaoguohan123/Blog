#pragma once

#include <ntifs.h>
#include <fwpmk.h>
#include <fwpsk.h>
#define INITGUID
#include <guiddef.h>
#include <fwpmu.h>

VOID UnInitWfp();
NTSTATUS InitializeWfp(PDRIVER_OBJECT pDriver);