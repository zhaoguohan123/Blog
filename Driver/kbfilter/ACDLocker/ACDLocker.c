/*--
Module Name:

    ACDLocker.c

Abstract:

Environment:

    Kernel mode only.

Notes:


--*/

#include "ACDLocker.h"


PDEVICE_OBJECT g_FilterCDO=NULL;

NTSTATUS DriverEntry (PDRIVER_OBJECT, PUNICODE_STRING);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, ACDLocker_AddDevice)
#pragma alloc_text (PAGE, ACDLocker_CreateClose)
#pragma alloc_text (PAGE, ACDLocker_InternIoCtl)
#pragma alloc_text (PAGE, ACDLocker_Unload)
#pragma alloc_text (PAGE, ACDLocker_DispatchPassThrough)
#pragma alloc_text (PAGE, ACDLocker_PnP)
#pragma alloc_text (PAGE, ACDLocker_Power)
#endif

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

    Initialize the entry points of the driver.

--*/
{
	NTSTATUS status=STATUS_SUCCESS;
    ULONG i=0;
    UNICODE_STRING nameString;
	UNICODE_STRING DosNameString;
	PCDO_DEVICE_EXTENSION deviceExtension=NULL;

    UNREFERENCED_PARAMETER (RegistryPath);

	// create cdo device object
    RtlInitUnicodeString( &nameString, NT_ACD_KBD_FILTER );
    status = IoCreateDevice( DriverObject,
                             sizeof(CDO_DEVICE_EXTENSION),
                             &nameString,
                             FILE_DEVICE_KEYBOARD,
                             FILE_DEVICE_SECURE_OPEN,
                             FALSE,
                             &g_FilterCDO);

    if ( !NT_SUCCESS( status ) ) {

        DebugPrint(("[Kbd]: IoCreateDevice failed with status 0x%x. \n", status) );
        return status;
    }

	// create symbolic link
    RtlInitUnicodeString(&DosNameString, DOS_ACD_KBD_FILTER);

    status = IoCreateSymbolicLink(&DosNameString, &nameString);

    if ( !NT_SUCCESS(status) )
    {
		DebugPrint(("[Kbd]: IoCreateSymbolicLink failed\n"));
    }

	deviceExtension = g_FilterCDO->DeviceExtension;
	RtlZeroMemory( deviceExtension, sizeof(CDO_DEVICE_EXTENSION) );
	deviceExtension->FilterMode = FALSE;
	RtlZeroMemory(&(deviceExtension->keyData1), sizeof(KEYBOARD_INPUT_DATA));
	RtlZeroMemory(&(deviceExtension->keyData2), sizeof(KEYBOARD_INPUT_DATA));
	deviceExtension->bSkip = FALSE;

    // 
    // Fill in all the dispatch entry points with the pass through function
    // and the explicitly fill in the functions we are going to intercept
    // 
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = ACDLocker_DispatchPassThrough;
    }

   /* DriverObject->MajorFunction [IRP_MJ_CREATE] =*/
    DriverObject->MajorFunction [IRP_MJ_CLOSE] =        ACDLocker_CreateClose;
    DriverObject->MajorFunction [IRP_MJ_PNP] =          ACDLocker_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER] =        ACDLocker_Power;
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] =
                                                        ACDLocker_InternIoCtl;
    //
    // If you are planning on using this function, you must create another
    // device object to send the requests to.  Please see the considerations 
    // comments for ACDLocker_DispatchPassThrough for implementation details.
    //
    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = ACDLocker_Ioctl;

    DriverObject->DriverUnload = ACDLocker_Unload;
    DriverObject->DriverExtension->AddDevice = ACDLocker_AddDevice;

    return STATUS_SUCCESS;
}

NTSTATUS
ACDLocker_AddDevice(
    __in PDRIVER_OBJECT   Driver,
    __in PDEVICE_OBJECT   PDO
    )
{
    PDEVICE_EXTENSION        devExt;
    IO_ERROR_LOG_PACKET      errorLogEntry;
    PDEVICE_OBJECT           device;
    NTSTATUS                 status = STATUS_SUCCESS;

    PAGED_CODE();

    status = IoCreateDevice(Driver,                   
                            sizeof(DEVICE_EXTENSION), 
                            NULL,                    
                            FILE_DEVICE_KEYBOARD,   
                            0,                     
                            FALSE,                
                            &device              
                            );

    if (!NT_SUCCESS(status)) {
        return (status);
    }

    RtlZeroMemory(device->DeviceExtension, sizeof(DEVICE_EXTENSION));

    devExt = (PDEVICE_EXTENSION) device->DeviceExtension;
    devExt->TopOfStack = IoAttachDeviceToDeviceStack(device, PDO);

    if (devExt->TopOfStack == NULL) {
        IoDeleteDevice(device);
        return STATUS_DEVICE_NOT_CONNECTED; 
    }

    
    ASSERT(devExt->TopOfStack);

    devExt->Self =          device;
    devExt->PDO =           PDO;
    devExt->DeviceState =   PowerDeviceD0;

    devExt->SurpriseRemoved = FALSE;
    devExt->Removed =         FALSE;
    devExt->Started =         FALSE;

    device->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
    device->Flags &= ~DO_DEVICE_INITIALIZING;

    return status;
}

NTSTATUS
ACDLocker_Complete(
    __in PDEVICE_OBJECT   DeviceObject,
    __in PIRP             Irp,
    __in_opt PVOID        Context
    )
/*++
Routine Description:

    Generic completion routine that allows the driver to send the irp down the 
    stack, catch it on the way up, and do more processing at the original IRQL.
    
--*/
{
    PKEVENT  event;

    event = (PKEVENT) Context;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    //
    // We could switch on the major and minor functions of the IRP to perform
    // different functions, but we know that Context is an event that needs
    // to be set.
    //
    KeSetEvent(event, 0, FALSE);

    //
    // Allows the caller to use the IRP after it is completed
    //
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
ACDLocker_CreateClose (
    __in PDEVICE_OBJECT  DeviceObject,
    __in PIRP            Irp
    )
/*++
Routine Description:

    Maintain a simple count of the creates and closes sent against this device
    
--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PDEVICE_EXTENSION   devExt;

    PAGED_CODE();

	if( DeviceObject == g_FilterCDO )
	{
		ACDLocker_CompleteRequest( Irp, STATUS_SUCCESS, 0);
		return STATUS_SUCCESS;
	}

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    status = Irp->IoStatus.Status;

    switch (irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
    
        if (NULL == devExt->UpperConnectData.ClassService) {
            //
            // No Connection yet.  How can we be enabled?
            //
            status = STATUS_INVALID_DEVICE_STATE;
        }
        else if ( 1 == InterlockedIncrement(&devExt->EnableCount)) {
            //
            // first time enable here
            //
        }
        else {
            //
            // More than one create was sent down
            //
        }
    
        break;

    case IRP_MJ_CLOSE:

        if (0 == InterlockedDecrement(&devExt->EnableCount)) {
            //
            // successfully closed the device, do any appropriate work here
            //
        }

        break;
    }

    Irp->IoStatus.Status = status;

    //
    // Pass on the create and the close
    //
    return ACDLocker_DispatchPassThrough(DeviceObject, Irp);
}

NTSTATUS
ACDLocker_DispatchPassThrough(
        __in PDEVICE_OBJECT DeviceObject,
        __in PIRP Irp
        )
/*++
Routine Description:

    Passes a request on to the lower driver.
     
Considerations:
     
    If you are creating another device object (to communicate with user mode
    via IOCTLs), then this function must act differently based on the intended 
    device object.  If the IRP is being sent to the solitary device object, then
    this function should just complete the IRP (becuase there is no more stack
    locations below it).  If the IRP is being sent to the PnP built stack, then
    the IRP should be passed down the stack. 
    
    These changes must also be propagated to all the other IRP_MJ dispatch
    functions (create, close, cleanup, etc) as well!

--*/
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    PAGED_CODE();

	if( DeviceObject == g_FilterCDO )
	{
		ACDLocker_CompleteRequest( Irp, STATUS_SUCCESS, 0);
		return STATUS_SUCCESS;
	}

    //
    // Pass the IRP to the target
    //
    IoSkipCurrentIrpStackLocation(Irp);
        
    return IoCallDriver(((PDEVICE_EXTENSION) DeviceObject->DeviceExtension)->TopOfStack, Irp);
}           

NTSTATUS
ACDLocker_InternIoCtl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:
    
    IOCTL_INTERNAL_KEYBOARD_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.
                                      
    IOCTL_INTERNAL_I8042_HOOK_KEYBOARD:
        Add in the necessary function pointers and context values so that we can
        alter how the ps/2 keyboard is initialized.  
                                            
    NOTE:  Handling IOCTL_INTERNAL_I8042_HOOK_KEYBOARD is *NOT* necessary if 
           all you want to do is filter KEYBOARD_INPUT_DATAs.  You can remove
           the handling code and all related device extension fields and 
           functions to conserve space.
                                         
Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION              irpStack;
    PDEVICE_EXTENSION               devExt;
    PINTERNAL_I8042_HOOK_KEYBOARD   hookKeyboard; 
    KEVENT                          event;
    PCONNECT_DATA                   connectData;
    NTSTATUS                        status = STATUS_SUCCESS;

    PAGED_CODE();

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    Irp->IoStatus.Information = 0;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    //
    // Connect a keyboard class device driver to the port driver.
    //
    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
        //
        // Only allow one connection.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }
        else if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(CONNECT_DATA)) {
            //
            // invalid buffer
            //
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Copy the connection parameters to the device extension.
        //
        connectData = ((PCONNECT_DATA)
            (irpStack->Parameters.DeviceIoControl.Type3InputBuffer));

        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a keyboard packet is reported
        // to the system, ACDLocker_ServiceCallback will be called
        //
        connectData->ClassDeviceObject = devExt->Self;
        connectData->ClassService = ACDLocker_ServiceCallback;

        break;

    //
    // Disconnect a keyboard class device driver from the port driver.
    //
    case IOCTL_INTERNAL_KEYBOARD_DISCONNECT:

        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;

        status = STATUS_NOT_IMPLEMENTED;
        break;

    default:
        break;
    }

    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    return ACDLocker_DispatchPassThrough(DeviceObject, Irp);
}

NTSTATUS
ACDLocker_PnP(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for plug and play irps 

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION           devExt; 
    PIO_STACK_LOCATION          irpStack;
    NTSTATUS                    status = STATUS_SUCCESS;
    KIRQL                       oldIrql;
    KEVENT                      event;        

    PAGED_CODE();

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->MinorFunction) {
    case IRP_MN_START_DEVICE: {

        //
        // The device is starting.
        //
        // We cannot touch the device (send it any non pnp irps) until a
        // start device has been passed down to the lower drivers.
        //
        IoCopyCurrentIrpStackLocationToNext(Irp);
        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE
                          );

        IoSetCompletionRoutine(Irp,
                               (PIO_COMPLETION_ROUTINE) ACDLocker_Complete, 
                               &event,
                               TRUE,
                               TRUE,
                               TRUE); // No need for Cancel

        status = IoCallDriver(devExt->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            KeWaitForSingleObject(
               &event,
               Executive, // Waiting for reason of a driver
               KernelMode, // Waiting in kernel mode
               FALSE, // No allert
               NULL); // No timeout
            status = Irp->IoStatus.Status;
        }

        if (NT_SUCCESS(status)) {
            //
            // As we are successfully now back from our start device
            // we can do work.
            //
            devExt->Started = TRUE;
            devExt->Removed = FALSE;
            devExt->SurpriseRemoved = FALSE;
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completetion routine with MORE_PROCESSING_REQUIRED.
        //
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;
    }

    case IRP_MN_SURPRISE_REMOVAL:
        //
        // Same as a remove device, but don't call IoDetach or IoDeleteDevice
        //
        devExt->SurpriseRemoved = TRUE;

        // Remove code here

        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
        break;

    case IRP_MN_REMOVE_DEVICE:
        
        devExt->Removed = TRUE;

        // remove code here
        Irp->IoStatus.Status = STATUS_SUCCESS;
        
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);

        IoDetachDevice(devExt->TopOfStack); 
        IoDeleteDevice(DeviceObject);

        break;

    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_CANCEL_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: 
    case IRP_MN_STOP_DEVICE:
    case IRP_MN_QUERY_DEVICE_RELATIONS:
    case IRP_MN_QUERY_INTERFACE:
    case IRP_MN_QUERY_CAPABILITIES:
    case IRP_MN_QUERY_DEVICE_TEXT:
    case IRP_MN_QUERY_RESOURCES:
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
    case IRP_MN_READ_CONFIG:
    case IRP_MN_WRITE_CONFIG:
    case IRP_MN_EJECT:
    case IRP_MN_SET_LOCK:
    case IRP_MN_QUERY_ID:
    case IRP_MN_QUERY_PNP_DEVICE_STATE:
    default:
        //
        // Here the filter driver might modify the behavior of these IRPS
        // Please see PlugPlay documentation for use of these IRPs.
        //
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
        break;
    }

    return status;
}

NTSTATUS
ACDLocker_Power(
    __in PDEVICE_OBJECT    DeviceObject,
    __in PIRP              Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for power irps   Does nothing except
    record the state of the device.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION  irpStack;
    PDEVICE_EXTENSION   devExt;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

    PAGED_CODE();

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    powerType = irpStack->Parameters.Power.Type;
    powerState = irpStack->Parameters.Power.State;

    switch (irpStack->MinorFunction) {
    case IRP_MN_SET_POWER:
        if (powerType  == DevicePowerState) {
            devExt->DeviceState = powerState.DeviceState;
        }

    case IRP_MN_POWER_SEQUENCE:
    case IRP_MN_WAIT_WAKE:
    case IRP_MN_QUERY_POWER:
    default:
        break;
    }

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(devExt->TopOfStack, Irp);
}

VOID
ACDLocker_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are keyboard packets to report to the RIT.  You can do 
    anything you like to the packets.  For instance:
    
    o Drop a packet altogether
    o Mutate the contents of a packet 
    o Insert packets into the stream 
                    
Arguments:

    DeviceObject - Context passed during the connect IOCTL
    
    InputDataStart - First packet to be reported
    
    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart
    
    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt=NULL;
	PCDO_DEVICE_EXTENSION deviceExtension=NULL;
	
    PIO_STACK_LOCATION        IrpSp;
    PKEYBOARD_INPUT_DATA      KeyData;
    int                       numKeys, i;
	USHORT keyVal1 = 0;
	USHORT keyVal2 = 0;
	BOOLEAN bCopy = FALSE;

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

	deviceExtension = (PCDO_DEVICE_EXTENSION) g_FilterCDO->DeviceExtension;


	if( deviceExtension->FilterMode == TRUE )
	{
		DbgPrint("\nACDLocker: InputDataStart->MakeCode : %d, InputDataStart->Flags : %d \n",InputDataStart->MakeCode, InputDataStart->Flags);
		if(InputDataStart->MakeCode != 0x2a) {
			
			do{
				if(deviceExtension->keyData1.MakeCode != 0 || deviceExtension->keyData2.MakeCode != 0) {
					if((InputDataStart->MakeCode != 0x1d) && (InputDataStart->MakeCode != 0x38) && (InputDataStart->MakeCode != 0x53))
					{
						DbgPrint("\nACDLocker: 1. Initializing global variables\n");
						RtlZeroMemory(&(deviceExtension->keyData1), sizeof(KEYBOARD_INPUT_DATA));
						RtlZeroMemory(&(deviceExtension->keyData2), sizeof(KEYBOARD_INPUT_DATA));
						deviceExtension->bSkip = FALSE;
						break;
					}
				}

				if(deviceExtension->bSkip == TRUE) {
					if((InputDataStart->Flags != 1) && (InputDataStart->Flags != 3)) {
						*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
						DbgPrint("\nACDLocker: 2. Return\n");
						return;
					} else {
						DbgPrint("\nACDLocker: 3. Initializing global variables\n");
						RtlZeroMemory(&(deviceExtension->keyData1), sizeof(KEYBOARD_INPUT_DATA));
						RtlZeroMemory(&(deviceExtension->keyData2), sizeof(KEYBOARD_INPUT_DATA));
						deviceExtension->bSkip = FALSE;
						break;
					}			
				}

				if(deviceExtension->keyData1.MakeCode != 0 && deviceExtension->keyData2.MakeCode != 0) {
					keyVal1 = deviceExtension->keyData1.MakeCode;
					keyVal2 = deviceExtension->keyData2.MakeCode;
					
					if((keyVal1 == 0x1d && keyVal2 == 0x38) || 
						(keyVal1 == 0x38 && keyVal2 == 0x1d)) {
						
						if(InputDataStart->MakeCode == 0x53)
							deviceExtension->bSkip = TRUE;
						
					} else if((keyVal1 == 0x1d && keyVal2 == 0x53) ||
						(keyVal1 == 0x53 && keyVal2 == 0x1d)) {
						
						if(InputDataStart->MakeCode == 0x38)
							deviceExtension->bSkip = TRUE;
						
					} else if((keyVal1 == 0x53 && keyVal2 == 0x38) ||
						(keyVal1 == 0x38 && keyVal2 == 0x53)) {
						
						if(InputDataStart->MakeCode == 0x1d)
							deviceExtension->bSkip = TRUE;
					}
					DbgPrint("\nACDLocker: 4. Returns\n");
					*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
					return;
				}

				if(InputDataStart->MakeCode == 0x1d) {
					DbgPrint("\nACDLocker: Pressed DEL\n");
					bCopy  = TRUE;
				} else if(InputDataStart->MakeCode == 0x38) {
					DbgPrint("\nACDLocker: Pressed ALT\n");
					bCopy  = TRUE;
				} else if(InputDataStart->MakeCode == 0x53) {
					DbgPrint("\nACDLocker: Pressed CTRL\n");
					bCopy  = TRUE;
				} else if(InputDataStart->MakeCode == 0x2a) {
					DbgPrint("\nACDLocker: Pressed Special Key\n");
				}
				
				if(bCopy == TRUE) {
					if(deviceExtension->keyData1.MakeCode == 0) {
						RtlCopyMemory(&(deviceExtension->keyData1), InputDataStart, sizeof(KEYBOARD_INPUT_DATA));
						DbgPrint("\nACDLocker: 5. Copied 1 datastructure\n");
					}
					else {
						RtlCopyMemory(&(deviceExtension->keyData2), InputDataStart, sizeof(KEYBOARD_INPUT_DATA));
						DbgPrint("\nACDLocker: 6. Copied 2 datastructure\n");
					}
				}		
				DbgPrint("\nACDLocker: 7. Break\n");	
				break;
			} while(1);
		}
	}
		
//		*InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
			//	return;
	
    (*(PSERVICE_CALLBACK_ROUTINE) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed);
}

VOID
ACDLocker_Unload(
   __in PDRIVER_OBJECT Driver
   )
/*++

Routine Description:

   Free all the allocated resources associated with this driver.

Arguments:

   DriverObject - Pointer to the driver object.

Return Value:

   None.

--*/

{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Driver);

    ASSERT(NULL == Driver->DeviceObject);
}


NTSTATUS
ACDLocker_Ioctl (
    __in PDEVICE_OBJECT  DeviceObject,
    __in PIRP            Irp
    )
/*++
Routine Description:

    Ioctls sent from user mode application are handled.
    
--*/
{
    PIO_STACK_LOCATION  irpStack=NULL;
    NTSTATUS            status=STATUS_SUCCESS;
    PCDO_DEVICE_EXTENSION	deviceExtension=NULL;
	ULONG				IoControlCode=0;
	PVOID				InputBuffer=NULL;
	ULONG				InputBufferLength=0;
	PULONG				Key=NULL;

    PAGED_CODE();

	if( DeviceObject != g_FilterCDO )
	{
		return ACDLocker_DispatchPassThrough(DeviceObject, Irp);
	}

	irpStack = IoGetCurrentIrpStackLocation(Irp);
    deviceExtension = (PCDO_DEVICE_EXTENSION) DeviceObject->DeviceExtension;
	IoControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
	InputBuffer = Irp->AssociatedIrp.SystemBuffer;
	InputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

	// Allow IOCTLs only if we get our key
	Key = InputBuffer;
	if( InputBufferLength < sizeof(ULONG) || *Key != ACD_KEY )
	{
		ACDLocker_CompleteRequest( Irp, STATUS_INVALID_DEVICE_REQUEST, 0 );
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	InputBuffer = ((PULONG)InputBuffer) + 1;
	InputBufferLength = InputBufferLength - sizeof(ULONG);

    switch ( IoControlCode ) 
	{
	case IOCTL_ACD_KBDFILTER_MODE:
		if( InputBufferLength < sizeof(BOOLEAN) )
		{
			ACDLocker_CompleteRequest( Irp, STATUS_INVALID_DEVICE_REQUEST, 0 );
			return STATUS_INVALID_DEVICE_REQUEST;
		}
		deviceExtension->FilterMode = *((PBOOLEAN)InputBuffer);
		break;
	default:
		DebugPrint(("[Kbd]: Unknown IOCTL\n"));
		break;
	}

	ACDLocker_CompleteRequest( Irp, STATUS_SUCCESS, 0 );
	return STATUS_SUCCESS;
}

NTSTATUS
ACDLocker_CompleteRequest(
	 __in PIRP			Irp,
	 __in NTSTATUS		Status,
	 __in ULONG			Information
	 )
/*++

--*/
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;

	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return STATUS_SUCCESS;
}