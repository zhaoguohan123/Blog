#ifndef _HOTKEYKRNL_H_
#define _HOTKEYKRNL_H_

#define DELAY_ONE_MICROSECOND  (-10)
#define DELAY_ONE_MILLISECOND  (DELAY_ONE_MICROSECOND * 1000)

#define KBD_DRIVER_NAME         L"\\Driver\\kbdclass"
#define MEM_TAG                 'VXer'

// Copy from windows SDK
#define MOD_ALT                 0x0001
#define MOD_CONTROL             0x0002
#define MOD_SHIFT               0x0004
#define MOD_WIN                 0x0008


typedef struct _DEVICE_EXTENSION {                                       
    ULONG NodeSize;
    PDEVICE_OBJECT pFilterDeviceObject;
    PDEVICE_OBJECT TargetDeviceObject;
    PDEVICE_OBJECT LowerDeviceObject;
    UNICODE_STRING ustrSymbolicName; 
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS
HotKeyKrnlDispatchDeviceControl (
    __in struct _DEVICE_OBJECT  *DeviceObject,
    __in struct _IRP  *Irp
    );

NTSTATUS
ObReferenceObjectByName(
    PUNICODE_STRING ObjectName,
    ULONG Attributes,
    PACCESS_STATE AccessState,
    ACCESS_MASK DesiredAccess,
    POBJECT_TYPE ObjectType,
    KPROCESSOR_MODE AccessMode,
    PVOID ParseContext,
    PVOID *Object
    );

NTSTATUS 
HotKeyKrnlAttachDevices( 
    IN PDRIVER_OBJECT DriverObject, 
    IN PUNICODE_STRING RegistryPath 
    );

VOID 
HotKeyKrnlDetach(
    IN PDEVICE_OBJECT pDeviceObject
    );

BOOLEAN
CancelKeyboardIrp(
    IN PIRP Irp
    );

VOID 
HotKeyKrnlUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
HotKeyKrnlDispatchGeneral( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    );

NTSTATUS 
HotKeyKrnlPower( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    );

NTSTATUS 
HotKeyKrnlPnP( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    );

NTSTATUS 
HotKeyKrnlReadComplete( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp, 
    IN PVOID Context 
    );

NTSTATUS 
HotKeyKrnlDispatchRead( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp
    );

NTSTATUS
HotKeyKrnlDispatchDeviceControl(
    __in struct _DEVICE_OBJECT  *DeviceObject,
    __in struct _IRP  *Irp
    );
    
#endif  /* _HOTKEYKRNL_H_ */