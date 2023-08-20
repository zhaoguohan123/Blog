/*++
Module Name:

    ACDLocker.h

Abstract:

    This module contains the common private declarations for the keyboard ALT+CTRL+DEL disable function

Environment:

    kernel mode only

Notes:


Revision History:


--*/

#ifndef ACDLocker_H
#define ACDLocker_H

#include "ntddk.h"
#include "kbdmou.h"
#include <ntddkbd.h>
#include <ntdd8042.h>

#if DBG

#define TRAP()                      DbgBreakPoint()
#define DbgRaiseIrql(_x_,_y_)       KeRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_)           KeLowerIrql(_x_)
#define DebugPrint(_x_) DbgPrint _x_

#else   // DBG

#define TRAP()
#define DbgRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_)

#define DebugPrint(_x_) 

#endif

#define MIN(_A_,_B_) (((_A_) < (_B_)) ? (_A_) : (_B_))

#define NT_ACD_KBD_FILTER               L"\\Device\\ACDLocker"
#define DOS_ACD_KBD_FILTER               L"\\DosDevices\\DosACDLocker"

//
// Macro definition for defining IOCTL and FSCTL function control codes.  Note
// that function codes 0-2047 are reserved for Microsoft Corporation, and
// 2048-4095 are reserved for customers.
//

#define ACD_IOCTL_INDEX  0x830

#define IOCTL_ACD_KBDFILTER_MODE				CTL_CODE(FILE_DEVICE_KEYBOARD,  \
												ACD_IOCTL_INDEX+18,  \
												METHOD_BUFFERED,       \
												FILE_ANY_ACCESS)

#define ACD_KEY 3847												
												
typedef struct _CDO_DEVICE_EXTENSION
{
	//
	// Keyboard filtering is on or off
	//
	BOOLEAN					FilterMode;
	KEYBOARD_INPUT_DATA   	keyData1;
	KEYBOARD_INPUT_DATA   	keyData2;
	BOOLEAN 				bSkip;

}CDO_DEVICE_EXTENSION,*PCDO_DEVICE_EXTENSION;

typedef struct _DEVICE_EXTENSION
{
    //
    // A backpointer to the device object for which this is the extension
    //
    PDEVICE_OBJECT  Self;

    //
    // "THE PDO"  (ejected by the root bus or ACPI)
    //
    PDEVICE_OBJECT  PDO;

    //
    // The top of the stack before this filter was added.  AKA the location
    // to which all IRPS should be directed.
    //
    PDEVICE_OBJECT  TopOfStack;

    //
    // Number of creates sent down
    //
    LONG EnableCount;

    //
    // The real connect data that this driver reports to
    //
    CONNECT_DATA UpperConnectData;

    //
    // Previous initialization and hook routines (and context)
    //                               
    PVOID UpperContext;
    PI8042_KEYBOARD_INITIALIZATION_ROUTINE UpperInitializationRoutine;
    PI8042_KEYBOARD_ISR UpperIsrHook;

    //
    // Write function from within AeKbd_IsrHook
    //
    IN PI8042_ISR_WRITE_PORT IsrWritePort;

    //
    // Queue the current packet (ie the one passed into AeKbd_IsrHook)
    //
    IN PI8042_QUEUE_PACKET QueueKeyboardPacket;

    //
    // Context for IsrWritePort, QueueKeyboardPacket
    //
    IN PVOID CallContext;

    //
    // current power state of the device
    //
    DEVICE_POWER_STATE  DeviceState;

    BOOLEAN         Started;
    BOOLEAN         SurpriseRemoved;
    BOOLEAN         Removed;
	

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Prototypes
//

DRIVER_ADD_DEVICE
ACDLocker_AddDevice;

DRIVER_DISPATCH
ACDLocker_CreateClose;

DRIVER_DISPATCH
ACDLocker_DispatchPassThrough;
   
DRIVER_DISPATCH
ACDLocker_InternIoCtl;

DRIVER_DISPATCH
ACDLocker_PnP;

DRIVER_DISPATCH
ACDLocker_Power;

IO_COMPLETION_ROUTINE
ACDLocker_Complete;


VOID
ACDLocker_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

DRIVER_UNLOAD
ACDLocker_Unload;


NTSTATUS
ACDLocker_Ioctl (
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP            Irp
    );

NTSTATUS
ACDLocker_CompleteRequest(
	 IN PIRP			Irp,
	 IN NTSTATUS		Status,
	 IN ULONG			Information
	 );

#endif  // AeKbd_H



