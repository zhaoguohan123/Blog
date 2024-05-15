#pragma once

#include <ntifs.h>
#include <windef.h>
#include <tdikrnl.h>
#include <tdi.h>

NTSTATUS InitTdiLayer(PDRIVER_OBJECT pDrv);
VOID UnInitTdiLayer(PDRIVER_OBJECT pDrv);
PDEVICE_OBJECT GetDoDeviceObj();
