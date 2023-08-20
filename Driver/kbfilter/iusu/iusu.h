///////////////////////////////////////////////////////////////////////////////
///
/// Copyright (c) 2009 - <company name here>
///
/// Original filename: ius.h
/// Project          : ius
/// Date of creation : <see ius.cpp>
/// Author(s)        : <see ius.cpp>
///
/// Purpose          : <see ius.cpp>
///
/// Revisions:         <see ius.cpp>
///
///////////////////////////////////////////////////////////////////////////////

// $Id$

#ifndef __IUS_H_VERSION__
#define __IUS_H_VERSION__ 100

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif


//#include "drvcommon.h"
//#include "drvversion.h"



#ifndef FILE_DEVICE_IUS
#define FILE_DEVICE_IUS 0x800
#endif

// Values defined for "Method"
// METHOD_BUFFERED
// METHOD_IN_DIRECT
// METHOD_OUT_DIRECT
// METHOD_NEITHER
// 
// Values defined for "Access"
// FILE_ANY_ACCESS
// FILE_READ_ACCESS
// FILE_WRITE_ACCESS

const ULONG IOCTL_IUS_OPERATION = CTL_CODE(FILE_DEVICE_IUS, 0x01, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA);

#endif // __IUS_H_VERSION__

#define PAGEDCODE code_seg("PAGE")
#define LOCKEDCODE code_seg()
#define INITCODE  code_seg("INIT")

#define PAGEDDATA  data_seg("PAGE")
#define LOCKEDDATA data_seg()
#define INITDATA data_seg("INIT")

#define arraysize(p) (sizeof(p)/sizeof((p)[0]))

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT pDevice;
	UNICODE_STRING ustrDeviceName;
	UNICODE_STRING ustrSymLinkName;
	ULONG uIrpPendingCount;
	BOOLEAN bAttached;
	BOOLEAN bThreadTerminate;
	PETHREAD pThreadObj;
	HANDLE hLogFile;
	KSPIN_LOCK Lock;
	LIST_ENTRY List;
	KSEMAPHORE Semaphore;
	KTIMER kTimer;
}DEVICE_EXTENSION,*PDEVICE_EXTENSION;


typedef struct _KEY_DATA
{
	LIST_ENTRY ListEntry;
	USHORT KeyData;
	USHORT KeyFlags;
}KEY_DATA,*PKEY_DATA;



NTSTATUS CompletionRoutine(IN PDEVICE_OBJECT DeviceObject,
						 IN PIRP Irp,
						 IN PVOID Context
						 );

NTSTATUS
Dispatch(
		 IN PDEVICE_OBJECT  DeviceObject,
		 IN PIRP  Irp
    );

NTSTATUS
Dispatched(
				  IN PDEVICE_OBJECT  DeviceObject,
				  IN PIRP  Irp
    );

VOID IusUnload(IN PDRIVER_OBJECT  DriverObject);