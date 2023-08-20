#include <wdm.h>
#include <ntddkbd.h>
#include "HotKeyKrnl.h"
#include "ioctlproto.h"


PDEVICE_OBJECT      g_pDevice  = NULL;
PIRP                PendingIrp = NULL;


ULONG               gKeyCount       = 0;
PHOTKEY_CFG         g_pHotkeyCfg    = NULL;
PKEVENT             g_pShareEvent   = NULL;
ULONG               g_ModifiersFlag = 0;
ULONG               g_NotifyIndex   = 0;



NTSTATUS 
DriverEntry( 
    IN PDRIVER_OBJECT   DriverObject, 
    IN PUNICODE_STRING  RegistryPath 
    )
{
    UNICODE_STRING      ustrDeviceName  = { 0 };
    PDEVICE_OBJECT      pDeviceObject   = NULL;
    PDEVICE_EXTENSION   pDeviceEx       = NULL;
    NTSTATUS            status;
    ULONG               i;
	DbgPrint("[shadow] Enter DriverEntry...\n");
    // Set some routines
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = HotKeyKrnlDispatchGeneral;
    }
    DriverObject->MajorFunction[IRP_MJ_READ] = HotKeyKrnlDispatchRead; 
    DriverObject->MajorFunction[IRP_MJ_POWER] = HotKeyKrnlPower; 
    DriverObject->MajorFunction[IRP_MJ_PNP] = HotKeyKrnlPnP;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HotKeyKrnlDispatchDeviceControl;
    DriverObject->DriverUnload = HotKeyKrnlUnload;

    // Attach
    status = HotKeyKrnlAttachDevices(DriverObject, RegistryPath);

    // Create device & symbol link
    RtlInitUnicodeString(&ustrDeviceName, L"\\Device\\KbdFilter");
    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), &ustrDeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
    if (NT_SUCCESS(status))
    {
        pDeviceObject->Flags |= DO_BUFFERED_IO;
        g_pDevice = pDeviceObject;
        RtlZeroMemory(pDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));  
        pDeviceEx = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension;
        RtlInitUnicodeString(&pDeviceEx->ustrSymbolicName, SYMBOL_LINK);
        IoCreateSymbolicLink(&pDeviceEx->ustrSymbolicName, &ustrDeviceName);
    }
    return status;
}



NTSTATUS 
HotKeyKrnlAttachDevices( 
    IN PDRIVER_OBJECT   DriverObject, 
    IN PUNICODE_STRING  RegistryPath 
    ) 
{ 
    NTSTATUS status = 0; 
    UNICODE_STRING uniNtNameString; 
    PDEVICE_EXTENSION   devExt; 
    PDEVICE_OBJECT pFilterDeviceObject = NULL; 
    PDEVICE_OBJECT pTargetDeviceObject = NULL; 
    PDEVICE_OBJECT pLowerDeviceObject  = NULL; 
    PDRIVER_OBJECT KbdDriverObject     = NULL;
    extern POBJECT_TYPE *IoDriverObjectType;    // Exported by ntoskrnl.exe, shit !

    // ref kbdclass driver object
    RtlInitUnicodeString(&uniNtNameString, KBD_DRIVER_NAME); 
    status = ObReferenceObjectByName(&uniNtNameString, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &KbdDriverObject);
    if(!NT_SUCCESS(status))
    {
        DbgPrint(("[shadow] ObReferenceObjectByName failed,cann't access Kbdclass.\n"));
        return status; 
    }
    else
    {
        ObDereferenceObject(KbdDriverObject);
    }

    // 绑定该驱动对象中的所有设备对象
    pTargetDeviceObject = KbdDriverObject->DeviceObject;
    while (pTargetDeviceObject) 
    {
        // 创建过滤设备
        status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL, pTargetDeviceObject->DeviceType, pTargetDeviceObject->Characteristics, FALSE, &pFilterDeviceObject);
        if (!NT_SUCCESS(status)) 
        { 
            DbgPrint(("[shadow] IoCreateDevice(filter device) failed.\n")); 
            return status; 
        } 
        // 绑定
        if(!(pLowerDeviceObject = IoAttachDeviceToDeviceStack(pFilterDeviceObject, pTargetDeviceObject)))
        { 
            DbgPrint(("[shadow] IoAttachDeviceToDeviceStack failed.\n")); 
            IoDeleteDevice(pFilterDeviceObject);
            pFilterDeviceObject = NULL; 
            return status; 
        }

        devExt = (PDEVICE_EXTENSION)(pFilterDeviceObject->DeviceExtension); 
        RtlZeroMemory(devExt, sizeof(DEVICE_EXTENSION)); 
        devExt->NodeSize = sizeof(DEVICE_EXTENSION); 
        devExt->pFilterDeviceObject = pFilterDeviceObject;
        devExt->TargetDeviceObject = pTargetDeviceObject; 
        devExt->LowerDeviceObject = pLowerDeviceObject; 

        pFilterDeviceObject->DeviceType = pLowerDeviceObject->DeviceType; 
        pFilterDeviceObject->Characteristics = pLowerDeviceObject->Characteristics; 
        pFilterDeviceObject->StackSize = pLowerDeviceObject->StackSize+1; 
        pFilterDeviceObject->Flags |= pLowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE); 

        pTargetDeviceObject = pTargetDeviceObject->NextDevice;
    }
    return status; 
} 



VOID 
HotKeyKrnlDetach(
    IN PDEVICE_OBJECT pDeviceObject
    ) 
{ 
	PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDeviceObject->DeviceExtension; 
	DbgPrint(("[shadow] Enter Detach...\n"));
	IoDetachDevice(pDevExt->LowerDeviceObject);
	IoDeleteDevice(pDeviceObject);   
	DbgPrint(("[shadow] Exit Detach...\n"));
}



BOOLEAN
CancelKeyboardIrp(
    IN PIRP Irp
    )
{
	// 这里有些判断应该不是必须的，不过还是小心点好
	if ( Irp->Cancel || Irp->CancelRoutine == NULL )
	{
		DbgPrint( "Can't Cancel the irp\n" );
		return FALSE;
	}
	if ( FALSE == IoCancelIrp( Irp ) )
	{
		DbgPrint( "IoCancelIrp() to failed\n" );
		return FALSE;
	}
	IoSetCancelRoutine(Irp, NULL);
	return TRUE; 
}



VOID 
HotKeyKrnlUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{ 
	PDEVICE_OBJECT  DeviceObject; 
	LARGE_INTEGER   lDelay;
	DbgPrint(("[shadow] Enter HotKeyKrnlUnload...\n"));
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	
    // Delete device we have created
	DeviceObject = DriverObject->DeviceObject;
	while (DeviceObject)
	{
		if (((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject)
			HotKeyKrnlDetach(DeviceObject);
		else
			IoDeleteDevice(DeviceObject);
		DeviceObject = DeviceObject->NextDevice;
	}
	if (gKeyCount > 0 && PendingIrp)
	{
		if (!CancelKeyboardIrp(PendingIrp))
			DbgPrint(("[shadow] Cancel irp failed\n"));
	}

    lDelay = RtlConvertLongToLargeInteger(100 * DELAY_ONE_MILLISECOND);
    while (gKeyCount) {
		KeDelayExecutionThread(KernelMode, FALSE, &lDelay);
    }
	DbgPrint(("[shadow] Exit HotKeyKrnlUnload...\n"));
}



NTSTATUS 
HotKeyKrnlDispatchGeneral( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    )
/*++

Routine Description:

    Very simple,if this is filter device's request,send to its under device progress directly.
    Otherwise,it must be our device used for communicate with user mode application,we should
    support some irp,so the CreateFile,CloseHandle called in the user mode call succeed.

--*/
{   
	if (DeviceObject == g_pDevice)
	{
		PIO_STACK_LOCATION  IrpSp = IoGetCurrentIrpStackLocation(Irp);
		switch (IrpSp->MajorFunction)
		{
		case IRP_MJ_CREATE:
            break;

		case IRP_MJ_CLOSE:
            DbgPrint(("[shadow] Enter IRP_MJ_CLOSE\n"));
            if (g_pHotkeyCfg) 
            {
                ExFreePoolWithTag(g_pHotkeyCfg, MEM_TAG);
                g_pHotkeyCfg = NULL;
            }
            if (g_pShareEvent)
            {
                ObDereferenceObject(g_pShareEvent);
                g_pShareEvent = NULL;
            }
            break;

		case IRP_MJ_CLEANUP:
			break;
		}
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
	}
    IoSkipCurrentIrpStackLocation(Irp); 
    return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp); 
} 

NTSTATUS 
HotKeyKrnlPower( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    )
{
	// IRP_MJ_POWER也是直接交给下层处理,不过具体方式与其它IRP不同
	PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension; 
	PoStartNextPowerIrp(Irp); 
	IoSkipCurrentIrpStackLocation(Irp); 
	// PoCallDriver,从VISTA开始变了
	return PoCallDriver(devExt->LowerDeviceObject, Irp); 
} 



NTSTATUS 
HotKeyKrnlPnP( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp 
    ) 
{ 
	PDEVICE_EXTENSION   devExt; 
	PIO_STACK_LOCATION  irpStack; 
	NTSTATUS status = STATUS_SUCCESS; 
    DbgPrint(("[shadow] Enter HotKeyKrnlPnP...\n"));
	devExt = (PDEVICE_EXTENSION)(DeviceObject->DeviceExtension); 
	irpStack = IoGetCurrentIrpStackLocation(Irp); 
	
	switch (irpStack->MinorFunction) 
	{ 
	case IRP_MN_REMOVE_DEVICE: 
		DbgPrint(("[shadow] Enter IRP_MJ_PNP - IRP_MN_REMOVE_DEVICE\n")); 
		IoSkipCurrentIrpStackLocation(Irp); 
		IoCallDriver(devExt->LowerDeviceObject, Irp); 
		IoDetachDevice(devExt->LowerDeviceObject); 
		IoDeleteDevice(DeviceObject); 
		status = STATUS_SUCCESS; 
		break; 
	default: 
		IoSkipCurrentIrpStackLocation(Irp); 
		status = IoCallDriver(devExt->LowerDeviceObject, Irp); 
	} 
	return status; 
}



NTSTATUS 
HotKeyKrnlReadComplete( 
    IN PDEVICE_OBJECT   DeviceObject, 
    IN PIRP             Irp, 
    IN PVOID            Context 
    )
{
	PKEYBOARD_INPUT_DATA    pKeyData;
    ULONG                   i = 0, j = 0;
	
	if(NT_SUCCESS(Irp->IoStatus.Status)) 
	{
		pKeyData = Irp->AssociatedIrp.SystemBuffer;
        DbgPrint(("[shadow] KEYBOARD_INPUT_DATA count: %d\n", Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA)));

        // If any key in the configuration is down,notify the application
        for(i = 0; i < Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA); i++)
        {
            if (g_pHotkeyCfg)
            {
                for (j = 0; j < g_pHotkeyCfg->ulItemCount; j++)
                {
                    switch (pKeyData[i].MakeCode)
                    {
                    case 0x38:  // ALT
                        if (!pKeyData[i].Flags)
                            g_ModifiersFlag |= MOD_ALT;
                        else
                            g_ModifiersFlag &= ~MOD_ALT;
                        continue;

                    case 0x1D:  // CTRL
                        if (!pKeyData[i].Flags)
                            g_ModifiersFlag |= MOD_CONTROL;
                        else
                            g_ModifiersFlag &= ~MOD_CONTROL;
                        continue;

                    case 0x2A:  // SHIFT
                    case 0x36:
                        if (!pKeyData[i].Flags)
                            g_ModifiersFlag |= MOD_SHIFT;
                        else
                            g_ModifiersFlag &= ~MOD_SHIFT;
                        continue;

                    } // end switch

                    if (pKeyData[i].MakeCode == g_pHotkeyCfg->CfgItem[j].nMakeCode && KEY_MAKE == pKeyData[i].Flags && g_ModifiersFlag == g_pHotkeyCfg->CfgItem[j].fsModifiers)
                    {
                        // attention please,g_NotifyIndex is a global variable,I don't do sync here
                        g_NotifyIndex = j;
                        KeSetEvent(g_pShareEvent, IO_NO_INCREMENT, FALSE);
                        break;
                    }
                } // end for
            }
            DbgPrint(("[shadow] Scan Code = %02X, KEY %s\n", pKeyData[i].MakeCode, pKeyData[i].Flags ? "UP" : "DOWN"));
        } // end for
	}
	gKeyCount--;

	if(Irp->PendingReturned)
		IoMarkIrpPending(Irp);
	return Irp->IoStatus.Status;
}


NTSTATUS 
HotKeyKrnlDispatchRead( 
    IN PDEVICE_OBJECT DeviceObject, 
    IN PIRP Irp
    ) 
/*++

Routine description:

    IRP_MJ_READ handler

Parameters:

    DeviceObject: 
        It must be our filter device
    
    Irp:

--*/
{ 
	PDEVICE_EXTENSION pDeviceExtension = NULL; 
	NTSTATUS          status = STATUS_SUCCESS;
	if (Irp->CurrentLocation == 1)
	{
        DbgPrint(("[shadow] I think you couldn't see this message.\n"));
		status = STATUS_INVALID_DEVICE_REQUEST; 
		Irp->IoStatus.Status = status; 
		Irp->IoStatus.Information = 0; 
		IoCompleteRequest(Irp, IO_NO_INCREMENT); 
		return status ; 
	} 

	gKeyCount++;
    PendingIrp = Irp;

	pDeviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, HotKeyKrnlReadComplete, DeviceObject, TRUE, TRUE, TRUE);
	return  IoCallDriver(pDeviceExtension->LowerDeviceObject, Irp);   
}


NTSTATUS
HotKeyKrnlDispatchDeviceControl(
    IN DEVICE_OBJECT  *DeviceObject,
    IN IRP            *Irp
    )
{
	if (DeviceObject == g_pDevice)
	{
		PIO_STACK_LOCATION  IrpSp = IoGetCurrentIrpStackLocation(Irp);
        PVOID               pInOutBuffer = Irp->AssociatedIrp.SystemBuffer;
        ULONG               ulInputBufferLength = 0;
        NTSTATUS            status;

		switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
		{
        case IOCTL_INITEVENT:
            status = ObReferenceObjectByHandle(*(HANDLE*)pInOutBuffer, EVENT_MODIFY_STATE, *ExEventObjectType, KernelMode, &g_pShareEvent, NULL);
            if (!NT_SUCCESS(status))
            {
                DbgPrint(("[shadow] ObReferenceObjectByHandle failed\n"));
            }
            break;

        case IOCTL_SETHOTKEYLIST:
            if (g_pHotkeyCfg) {
                ExFreePoolWithTag(g_pHotkeyCfg, MEM_TAG);
                g_pHotkeyCfg = NULL;
            }
            ulInputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
            g_pHotkeyCfg = ExAllocatePoolWithTag(PagedPool, ulInputBufferLength + sizeof(g_pHotkeyCfg->ulItemCount), MEM_TAG);
            if (g_pHotkeyCfg)
            {
                g_pHotkeyCfg->ulItemCount = ulInputBufferLength / sizeof(IOCTL_INPUTDATA);
                RtlCopyMemory(&g_pHotkeyCfg->CfgItem, pInOutBuffer, ulInputBufferLength);
            }
            break;

        case IOCTL_CLEARLIST:
            if (g_pHotkeyCfg) {
                ExFreePoolWithTag(g_pHotkeyCfg, MEM_TAG);
                g_pHotkeyCfg = NULL;
            }
            break;

        case IOCTL_CLEANUP:
            DbgPrint(("[shadow] Enter HotKeyKrnlDispatchDeviceControl - IOCTL_APPEXIT\n"));
            if (g_pHotkeyCfg) 
            {
                ExFreePoolWithTag(g_pHotkeyCfg, MEM_TAG);
                g_pHotkeyCfg = NULL;
            }
            if (g_pShareEvent)
            {
                ObDereferenceObject(g_pShareEvent);
                g_pShareEvent = NULL;
            }
            break;

        case IOCTL_GETHOTKEYINDEX:
            DbgPrint(("[shadow] Enter HotKeyKrnlDispatchDeviceControl - IOCTL_GETEVENTINDEX\n"));
            if (sizeof(ULONG) == IrpSp->Parameters.DeviceIoControl.OutputBufferLength)
            {
                *(ULONG*)pInOutBuffer = g_NotifyIndex;
                Irp->IoStatus.Status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(ULONG);
            }
            break;

		default:
			Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
			Irp->IoStatus.Information = 0;
			break;
		}
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_SUCCESS;
	}
	else
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp); 
	}
}
