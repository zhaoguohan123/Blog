#include "common.h"

PDEVICE_OBJECT gSalmonDeviceObject = NULL;
extern POBJECT_TYPE* IoDriverObjectType;

extern BOOLEAN g_DisableCad;               // ����ctrl + alt + del

PIRP
KeyboardClassDequeueRead(
    _In_ PCHAR DeviceExtension
)
/*++

Routine Description:
    Dequeues the next available read irp regardless of FileObject

Assumptions:
    DeviceExtension->SpinLock is already held (so no further sync is required).

  --*/
{
    ASSERT(NULL != DeviceExtension);

    PIRP nextIrp = NULL;

    LIST_ENTRY* ReadQueue = (LIST_ENTRY*)(DeviceExtension + READ_QUEUE_OFFSET_DE);

    while (!nextIrp && !IsListEmpty(ReadQueue)) {
        PDRIVER_CANCEL oldCancelRoutine;
        PLIST_ENTRY listEntry = RemoveHeadList(ReadQueue);

        //
        // Get the next IRP off the queue and clear the cancel routine
        //
        nextIrp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);
        oldCancelRoutine = IoSetCancelRoutine(nextIrp, NULL);

        //
        // IoCancelIrp() could have just been called on this IRP.
        // What we're interested in is not whether IoCancelIrp() was called
        // (ie, nextIrp->Cancel is set), but whether IoCancelIrp() called (or
        // is about to call) our cancel routine. To check that, check the result
        // of the test-and-set macro IoSetCancelRoutine.
        //
        if (oldCancelRoutine) {
            //
                //  Cancel routine not called for this IRP.  Return this IRP.
            //
            /*ASSERT(oldCancelRoutine == KeyboardClassCancel);*/
        }
        else {
            //
                // This IRP was just cancelled and the cancel routine was (or will
            // be) called. The cancel routine will complete this IRP as soon as
            // we drop the spinlock. So don't do anything with the IRP.
            //
                // Also, the cancel routine will try to dequeue the IRP, so make the
            // IRP's listEntry point to itself.
            //
            //ASSERT(nextIrp->Cancel);
            InitializeListHead(&nextIrp->Tail.Overlay.ListEntry);
            nextIrp = NULL;
        }
    }

    return nextIrp;
}

VOID
PocCancelOperation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    PIO_STACK_LOCATION IrpSp = NULL;

    PLIST_ENTRY listEntry = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    IoReleaseCancelSpinLock(Irp->CancelIrql);

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    listEntry = ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

    while (listEntry != &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead)
    {
        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

        if (KbdObj->KbdFileObject == IrpSp->FileObject)
        {
            break;
        }

        listEntry = listEntry->Flink;
    }

    if (NULL == KbdObj)
    {
        DbgPrint("KbdObj is NULL!");
        goto EXIT;
    }

    
    //Irp�ڴ棬KbdDeviceObject�ڴ��п����Ѿ����ͷ��ˣ��豸Ҳ���Ƴ��ˣ�ʹ�������ʶ��ֹPagefault
    ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

    KbdObj->IrpCancel = TRUE;
    KbdObj->KbdFileObject->DeviceObject = KbdObj->BttmDeviceObject;

    ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);

    //ͨ�������ڼ���ж��ʱ��IoCancelIrp(Irp)��Poc����IoCancelIrp(NewIrp)������Kbdclass���NewIrpҲ�᷵��
    if (NULL != KbdObj->NewIrp)
    {
        IoCancelIrp(KbdObj->NewIrp);
    }

EXIT:

    Irp->IoStatus.Status = STATUS_CANCELLED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

VOID
SalmonHandleReadThread(
    _In_ PVOID StartContext
)
{
    ASSERT(NULL != StartContext);

    NTSTATUS Status = 0;

    PIRP Irp = NULL, NewIrp = NULL;
    PIO_STACK_LOCATION IrpSp = NULL, NewIrpSp = NULL;

    PLIST_ENTRY listEntry = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    PIO_REMOVE_LOCK RemoveLock = NULL;
    PKEYBOARD_INPUT_DATA InputData = NULL;

    Irp = StartContext;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    listEntry = ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

    while (listEntry != &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead)
    {
        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

        if (KbdObj->KbdFileObject == IrpSp->FileObject)
        {
            break;
        }

        listEntry = listEntry->Flink;
    }

    if (NULL == KbdObj)
    {
        DbgPrint("KbdObj is null!");
        Status = STATUS_INVALID_PARAMETER;
        goto EXIT;
    }
    RemoveLock = (PIO_REMOVE_LOCK)((PCHAR)KbdObj->KbdDeviceObject->DeviceExtension + REMOVE_LOCK_OFFET_DE);

    if (Irp == KbdObj->RemoveLockIrp)
    {
        IoReleaseRemoveLock(RemoveLock, Irp);
        KbdObj->RemoveLockIrp = NULL;
    }

    // ����IRP
    NewIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_READ,
        KbdObj->KbdDeviceObject,
        Irp->AssociatedIrp.SystemBuffer,
        IrpSp->Parameters.Read.Length,
        &IrpSp->Parameters.Read.ByteOffset,
        &KbdObj->Event,
        &Irp->IoStatus);

    if (NULL == NewIrp)
    {
        DbgPrint("IoBuildSynchronousFsdRequest NewIrp failed..");
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }


    KeClearEvent(NewIrp->UserEvent);                                //UserEvent��IRP���������ʱ�򣬸��¼��ᱻ����
    NewIrp->Tail.Overlay.Thread = PsGetCurrentThread();
    NewIrp->Tail.Overlay.AuxiliaryBuffer = NULL;
    NewIrp->RequestorMode = KernelMode;
    NewIrp->PendingReturned = FALSE;
    NewIrp->Cancel = FALSE;
    NewIrp->CancelRoutine = NULL;

    
    // UserApcRoutine == win32kbase!rimInputApc����NT4��Win32kԴ�벻ͬ��Windows 10ʹ�õ���APC����Event,
    
    NewIrp->Tail.Overlay.OriginalFileObject = NULL;
    NewIrp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;
    NewIrp->Overlay.AsynchronousParameters.UserApcContext = NULL;

    NewIrpSp = IoGetNextIrpStackLocation(NewIrp);
    NewIrpSp->FileObject = KbdObj->KbdFileObject;
    NewIrpSp->Parameters.Read.Key = IrpSp->Parameters.Read.Key;


    ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

    KbdObj->NewIrp = NewIrp;

    ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);


    Status = IoCallDriver(KbdObj->KbdDeviceObject, NewIrp);

    if (!NT_SUCCESS(Status))
    {
        DbgPrint("IoCallDriver failed. Status = 0x%x.", Status);
    }

    if (STATUS_PENDING == Status)
    {
        KeWaitForSingleObject(
            NewIrp->UserEvent,
            WrUserRequest,
            KernelMode,
            TRUE,
            NULL);
    }


    if (KbdObj->IrpCancel)
    {
        if (NULL != KbdObj)
        {
            ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);
            KbdObj->SafeUnload = TRUE;
            ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
        }
        goto EXIT;
    }
    else
    {
        IoSetCancelRoutine(Irp, NULL);
    }


    if (STATUS_SUCCESS == Irp->IoStatus.Status &&
        0 != Irp->IoStatus.Information)
    {
        IrpSp->Parameters.Read.Length = (ULONG)Irp->IoStatus.Information;
        InputData = Irp->AssociatedIrp.SystemBuffer;

        for (InputData;
            (PCHAR)InputData < (PCHAR)Irp->AssociatedIrp.SystemBuffer + Irp->IoStatus.Information;
            InputData++)
        {
 /*           PocPrintScanCode(InputData);

            PocConfigureKeyMapping(InputData);*/
        }

        if (NULL != KbdObj)
        {
            ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

            KbdObj->SafeUnload = TRUE;

            ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
        }

        IoCompleteRequest(Irp, IO_KEYBOARD_INCREMENT);
    }
    else
    {
        if (NULL != KbdObj)
        {
            ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

            KbdObj->SafeUnload = TRUE;

            ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
        }

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
EXIT:
    PsTerminateSystemThread(Status);
}


NTSTATUS
ReadOperation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    
    NTSTATUS Status = 0;

    PIO_STACK_LOCATION IrpSp = NULL;

    PPOC_KBDCLASS_OBJECT KbdObj = NULL;
    PLIST_ENTRY listEntry = NULL;

    HANDLE ThreadHandle = NULL;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    if (IrpSp->Parameters.Read.Length == 0)
    {
        Status = STATUS_SUCCESS;
    }
    else if (IrpSp->Parameters.Read.Length % sizeof(KEYBOARD_INPUT_DATA))
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }
    else
    {
        Status = STATUS_PENDING;
    }

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;

    if (Status == STATUS_PENDING)
    {
        IoSetCancelRoutine(Irp, PocCancelOperation);

        if (Irp->Cancel)
        {
            Status = STATUS_CANCELLED;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);

            goto EXIT;
        }

        listEntry = ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

        while (listEntry != &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead)
        {
            KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

            if (KbdObj->KbdFileObject == IrpSp->FileObject)
            {
                ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

                KbdObj->SafeUnload = FALSE;

                ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
            }

            listEntry = listEntry->Flink;
        }
        IoMarkIrpPending(Irp);

        Status = PsCreateSystemThread(
            &ThreadHandle,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            SalmonHandleReadThread,
            Irp);

        if (!NT_SUCCESS(Status))
        {
            DbgPrint("PsCreateSystemThread PocHandleReadThread failed.Status = 0x%x.",Status);
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            goto EXIT;
        }

        if (NULL != ThreadHandle)
        {
            ZwClose(ThreadHandle);
            ThreadHandle = NULL;
        }

        return STATUS_PENDING;
    }
    else
    {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
EXIT:
    return Status;
}

NTSTATUS
DeviceControlOperation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    /*
    * CBaseInput::OnReadNotification
    */
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    NTSTATUS Status = 0;

    PIO_STACK_LOCATION IrpSp = NULL;

    PLIST_ENTRY listEntry = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    listEntry = ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

    while (listEntry != &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead)
    {
        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

        if (KbdObj->KbdFileObject == IrpSp->FileObject)
        {
            break;
        }

        listEntry = listEntry->Flink;
    }

    if (NULL == KbdObj)
    {
        DbgPrint("KbdObj is null.");
        Status = STATUS_INVALID_PARAMETER;
        goto EXIT;
    }

    ULONG major_code = IrpSp->MajorFunction;
    ULONG contol_code = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    //if (major_code == IRP_MJ_DEVICE_CONTROL)
    //{
    //    switch (contol_code)
    //    {
    //    case IOCTL_CODE_TO_DISABLE_ALL_KEY:
    //        DbgPrint("r3 notify disable kbd!");
    //        g_DisableKeyBoard = TRUE;
    //        Irp->IoStatus.Status = STATUS_SUCCESS;
    //        Irp->IoStatus.Information = 0;
    //        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    //        return STATUS_SUCCESS;

    //    case IOCTL_CODE_TO_ENABLE_ALL_KEY:
    //        DbgPrint("r3 notify enable kbd!");
    //        g_DisableKeyBoard = FALSE;
    //        Irp->IoStatus.Status = STATUS_SUCCESS;
    //        Irp->IoStatus.Information = 0;
    //        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    //        return STATUS_SUCCESS;

    //    case IOCTL_CODE_TO_ENABLE_CAD:
    //        DbgPrint("r3 notify enable cad!");
    //        g_DisableCad = FALSE;
    //        Irp->IoStatus.Status = STATUS_SUCCESS;
    //        Irp->IoStatus.Information = 0;
    //        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    //        return STATUS_SUCCESS;

    //    case IOCTL_CODE_TO_DISABLE_CAD:
    //        DbgPrint("r3 notify disable cad!");
    //        g_DisableCad = TRUE;
    //        Irp->IoStatus.Status = STATUS_SUCCESS;
    //        Irp->IoStatus.Information = 0;
    //        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    //        return STATUS_SUCCESS;

    //    case IOCTL_CODE_TO_CREATE_EVENT:
    //        /*
    //        *  �����Ϳͻ���ͨ�ŵ��¼�
    //        */
    //        DbgPrint("r3 notify create cad event!");
    //        if (g_pEvent != NULL)
    //        {
    //            DbgPrint("g_pEvent is not NULL, event has been exits!");
    //            goto EXIT;
    //        }

    //        g_pEvent = (PKEVENT)ExAllocatePool(NonPagedPool, sizeof(KEVENT));
    //        UNICODE_STRING ustrEventName = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\Troila_Kbd_fltr");
    //        g_pEvent = IoCreateNotificationEvent(&ustrEventName, &g_hEvent);
    //        if (g_pEvent == NULL)
    //        {
    //            DbgPrint("IoCreateSynchronizationEvent failed!");
    //            goto EXIT;
    //        }

    //        // �¼���ʼ��Ϊ�Ǵ���״̬
    //        (void)KeInitializeEvent(&g_hEvent, NotificationEvent, FALSE);
    //        Irp->IoStatus.Status = STATUS_SUCCESS;
    //        Irp->IoStatus.Information = 0;
    //        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    //        return STATUS_SUCCESS;
    //    default:
    //        break;
    //    }
    //}

    IoSkipCurrentIrpStackLocation(Irp);

    Status = IoCallDriver(KbdObj->KbdDeviceObject, Irp);

    if (!NT_SUCCESS(Status))
    {
        DbgPrint("IoCallDriver failed. Status = 0x%x.", Status);
    }

EXIT:
    return Status;
}

VOID
KbdObjListCleanup(
)
{
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;
    PLIST_ENTRY listEntry = NULL;

    PCHAR KbdDeviceExtension = NULL;
    PIO_REMOVE_LOCK RemoveLock = NULL;
    PKSPIN_LOCK SpinLock = NULL;
    KIRQL Irql = { 0 };

    PIRP Irp = NULL;

    ULONG ulHundredNanoSecond = 0;
    LARGE_INTEGER Interval = { 0 };

#pragma warning(push)
#pragma warning(disable:4996)
    ulHundredNanoSecond = 1 * 1000 * 1000;
    Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)

    while (!IsListEmpty(&((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead))
    {
        listEntry = ExInterlockedRemoveHeadList(
            &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead,
            &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjSpinLock);

        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);


        if (!KbdObj->IrpCancel)
        {
            KbdDeviceExtension = KbdObj->KbdDeviceObject->DeviceExtension;

            RemoveLock = (PIO_REMOVE_LOCK)((PCHAR)KbdObj->KbdDeviceObject->DeviceExtension + REMOVE_LOCK_OFFET_DE);
            SpinLock = (PKSPIN_LOCK)(KbdDeviceExtension + SPIN_LOCK_OFFSET_DE);

            while (TRUE)
            {
                KeAcquireSpinLock(SpinLock, &Irql);
                Irp = KeyboardClassDequeueRead(KbdDeviceExtension);
                KeReleaseSpinLock(SpinLock, Irql);

                if (NULL != Irp)
                {
                    break;
                }
            }


            KbdObj->KbdFileObject->DeviceObject = KbdObj->BttmDeviceObject;

            /*
            * ���IRP��Poc����PocHandleReadThread��������Kbdclass�ģ�
            * IoCompleteRequest�Ժ�PocHandleReadThread��KeWaitForSingleObject������ȴ���PocHandleReadThread�߳�Ҳ���˳�
            */
            Irp->IoStatus.Status = 0;
            Irp->IoStatus.Information = 0;

            IoReleaseRemoveLock(RemoveLock, Irp);
            IoCompleteRequest(Irp, IO_NO_INCREMENT);


            while (!KbdObj->SafeUnload)
            {
                KeDelayExecutionThread(KernelMode, FALSE, &Interval);
            }

            DbgPrint("[salmon] Safe to unload. gPocDeviceObject = %p KbdObj = %p KbdDeviceObject = %p.\nBttmDeviceObject = %p KbdFileObject = %p KbdFileObject->DeviceObject = %p.",
                gSalmonDeviceObject,
                    KbdObj,
                    KbdObj->KbdDeviceObject,
                    KbdObj->BttmDeviceObject,
                    KbdObj->KbdFileObject,
                    KbdObj->KbdFileObject->DeviceObject);
        }
        else
        {
            DbgPrint("[salmon]Device has been removed. gPocDeviceObject = %p KbdObj = %p.",gSalmonDeviceObject, KbdObj);
        }


        ExDeleteResourceLite(&KbdObj->Resource);

        if (NULL != KbdObj)
        {
            ExFreePoolWithTag(KbdObj, POC_NONPAGED_POOL_TAG);
            KbdObj = NULL;
        }
    }

    /*
    * ��PocIrpHookInitThread�˳�
    */
    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

VOID
Unload(
    _In_ PDRIVER_OBJECT  DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gIsUnloading = TRUE;

    KbdObjListCleanup();


    if (NULL != ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdDriverObject)
    {
        ObDereferenceObject(((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdDriverObject);
        ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdDriverObject = NULL;
    }

    //if (g_pEvent != NULL)
    //{
    //    (void)KeClearEvent(g_pEvent);

    //    (void)ZwClose(g_hEvent);

    //    g_pEvent = NULL;
    //    g_hEvent = NULL;
    //}

    //ɾ�������豸
    UNICODE_STRING kbd_syb = RTL_CONSTANT_STRING(CDO_SYB_NAME);
    if (NT_SUCCESS(IoDeleteSymbolicLink(&kbd_syb)))
    {
        DbgPrint("IoDeleteSymbolicLink failed. \n");
    }

    if (NULL != gSalmonDeviceObject)
    {
        IoDeleteDevice(gSalmonDeviceObject);
        gSalmonDeviceObject = NULL;
    }
}

NTSTATUS
DeviceClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS
DeviceCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}




VOID
SalmonIrpHookInitThread(
    _In_ PVOID StartContext
)
{
    UNREFERENCED_PARAMETER(StartContext);

    NTSTATUS Status = 0;

    PDEVICE_OBJECT KbdDeviceObject = NULL;

    PLIST_ENTRY listEntry = NULL;

    PCHAR KbdDeviceExtension = NULL;
    PIO_REMOVE_LOCK RemoveLock = NULL;
    PKSPIN_LOCK SpinLock = NULL;
    KIRQL Irql = { 0 };

    PIRP Irp = NULL;
    PIO_STACK_LOCATION IrpSp = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    HANDLE ThreadHandle = NULL;

    ULONG ulHundredNanoSecond = 0;
    LARGE_INTEGER Interval = { 0 };

#pragma warning(push)
#pragma warning(disable:4996)
    ulHundredNanoSecond = 1 * 1000 * 1000;
    Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)


    /*
    * 100ms��ʱɨ���Ƿ����¼����豸
    */
    while (!((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gIsUnloading)
    {
        KbdDeviceObject = ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdDriverObject->DeviceObject;

        /*
        * �жϼ����豸�Ƿ��Ѿ��ӵ���������
        */
        while (NULL != KbdDeviceObject)
        {
            listEntry = ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

            while (listEntry != &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead)
            {
                KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

                if (KbdObj->KbdDeviceObject == KbdDeviceObject)
                {
                    break;
                }

                listEntry = listEntry->Flink;
            }

            if (NULL != KbdObj && KbdObj->KbdDeviceObject == KbdDeviceObject)
            {
                KbdDeviceObject = KbdDeviceObject->NextDevice;
                continue;
            }

            /*
            * ��ʼ����Hook�ĳ�ʼ������
            */
            KbdDeviceExtension = KbdDeviceObject->DeviceExtension;
            RemoveLock = (PIO_REMOVE_LOCK)(KbdDeviceExtension + REMOVE_LOCK_OFFET_DE);
            SpinLock = (PKSPIN_LOCK)(KbdDeviceExtension + SPIN_LOCK_OFFSET_DE);

            /*
            * ��Kbdclass��IRP����DeviceExtension->ReadQueueȡ��IRP
            */
            while (TRUE)
            {
                KeAcquireSpinLock(SpinLock, &Irql);
                Irp = KeyboardClassDequeueRead(KbdDeviceExtension);
                KeReleaseSpinLock(SpinLock, Irql);

                if (NULL != Irp)
                {
                    break;
                }
            }

            IoSetCancelRoutine(Irp, PocCancelOperation);

            IrpSp = IoGetCurrentIrpStackLocation(Irp);


            KbdObj = ExAllocatePoolWithTag(NonPagedPool, sizeof(POC_KBDCLASS_OBJECT), POC_NONPAGED_POOL_TAG);

            if (NULL == KbdObj)
            {
                DbgPrint("ExAllocatePoolWithTag KbdObj failed.");
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto EXIT;
            }

            RtlZeroMemory(KbdObj, sizeof(POC_KBDCLASS_OBJECT));

            /*
            * KbdDeviceObject��KbdclassΪÿ���ײ��豸����ĵ��豸����
            * gBttmDeviceObject��Kbdclass�豸ջ��Ͳ���豸����ͨ��ΪPS/2��USB HID����
            */
            KbdObj->SafeUnload = FALSE;
            KbdObj->RemoveLockIrp = Irp;
            KbdObj->KbdFileObject = IrpSp->FileObject;
            KbdObj->BttmDeviceObject = IrpSp->FileObject->DeviceObject;
            KbdObj->KbdDeviceObject = KbdDeviceObject;

            KeInitializeEvent(&KbdObj->Event, SynchronizationEvent, FALSE);
            ExInitializeResourceLite(&KbdObj->Resource);

            /*
            * �滻FileObject->DeviceObjectΪgPocDeviceObject��
            * ����Win32k��IRP�ͻᷢ�����ǵ�Poc����
            */
            KbdObj->KbdFileObject->DeviceObject = gSalmonDeviceObject;

            gSalmonDeviceObject->StackSize = max(KbdObj->BttmDeviceObject->StackSize, gSalmonDeviceObject->StackSize);


            ExInterlockedInsertTailList(
                &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead,
                &KbdObj->ListEntry,
                &((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjSpinLock);

            KbdObj->InitSuccess = TRUE;
            DbgPrint("ExInterlockedInsertTailList gSalmonDeviceObject = % p KbdObj = % p KbdDeviceObject = % p.\nBttmDeviceObject = % p KbdFileObject = % p.",
                    gSalmonDeviceObject,
                    KbdObj,
                    KbdObj->KbdDeviceObject,
                    KbdObj->BttmDeviceObject,
                    KbdObj->KbdFileObject);

            Status = PsCreateSystemThread(
                &ThreadHandle,
                THREAD_ALL_ACCESS,
                NULL,
                NULL,
                NULL,
                SalmonHandleReadThread,
                Irp);

            if (!NT_SUCCESS(Status))
            {
                DbgPrint("PsCreateSystemThread PocHandleReadThread failed.Status =0x%x.", Status);
                goto EXIT;
            }

            if (NULL != ThreadHandle)
            {
                ZwClose(ThreadHandle);
                ThreadHandle = NULL;
            }

            KbdDeviceObject = KbdDeviceObject->NextDevice;
        }

        KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }

    Status = STATUS_SUCCESS;

EXIT:

    if (!NT_SUCCESS(Status) && NULL != KbdObj)
    {
        if (KbdObj->KbdFileObject->DeviceObject != KbdObj->BttmDeviceObject)
        {
            KbdObj->KbdFileObject->DeviceObject = KbdObj->BttmDeviceObject;
        }

        if (!KbdObj->InitSuccess)
        {
            ExDeleteResourceLite(&KbdObj->Resource);

            if (NULL != KbdObj)
            {
                ExFreePoolWithTag(KbdObj, POC_NONPAGED_POOL_TAG);
                KbdObj = NULL;
            }
        }
    }

    if (!NT_SUCCESS(Status) && NULL != Irp)
    {
        Irp->IoStatus.Status = 0;
        Irp->IoStatus.Information = 0;
        IoReleaseRemoveLock(RemoveLock, Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    PsTerminateSystemThread(Status);
}


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) 
{

    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS Status = 0;

    UNICODE_STRING DriverName = { 0 };
    PDRIVER_OBJECT kbdDriverObject = NULL;

    HANDLE ThreadHandle = NULL;

    RtlInitUnicodeString(&DriverName, KBD_DEV_NAME);

    Status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &DriverName,
        FILE_DEVICE_KEYBOARD,
        0,
        FALSE,
        &gSalmonDeviceObject);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("[salmon]IoCreateDevice failed. Status = 0x%x", Status);
        goto EXIT;
    }

    //���÷������ݻ������ķ�ʽ
    gSalmonDeviceObject->Flags |= DO_BUFFERED_IO;

    // �ҵ���������Kbdclass��DriverObject
    RtlInitUnicodeString(&DriverName, L"\\Driver\\Kbdclass");

    Status = ObReferenceObjectByName(
        &DriverName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        FILE_ALL_ACCESS,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        &kbdDriverObject);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("[salmon]ObReferenceObjectByName failed. Status = 0x%x", Status);
        goto EXIT;
    }
    ((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdDriverObject = kbdDriverObject;
    InitializeListHead(&((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjListHead);
    KeInitializeSpinLock(&((PDEVICE_EXTENSION)(gSalmonDeviceObject->DeviceExtension))->gKbdObjSpinLock);

    // ���������ķ�������
    UNICODE_STRING kbd_dev_name = RTL_CONSTANT_STRING(KBD_DEV_NAME);
    UNICODE_STRING kbd_syb = RTL_CONSTANT_STRING(CDO_SYB_NAME);
    
    Status = IoCreateSymbolicLink(&kbd_syb, &kbd_dev_name);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("[salmon]IoCreateSymbolicLink failed.");
        goto EXIT;
    }

    DriverObject->MajorFunction[IRP_MJ_READ] = ReadOperation;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlOperation;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceClose;

    DriverObject->DriverUnload = Unload;

    /*
    * ��������Hook��ʼ���߳�
    */
    Status = PsCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        SalmonIrpHookInitThread,
        NULL);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("[salmon]PsCreateSystemThread PocIrpHookInitThread failed. Status = 0x%x.", Status);
        goto EXIT;
    }

    if (NULL != ThreadHandle)
    {
        ZwClose(ThreadHandle);
        ThreadHandle = NULL;
    }

    Status = STATUS_SUCCESS;
EXIT:
    if (!NT_SUCCESS(Status) && NULL != gSalmonDeviceObject)
    {
        IoDeleteDevice(gSalmonDeviceObject);
        gSalmonDeviceObject = NULL;
    }

    if (!NT_SUCCESS(Status) && NULL != kbdDriverObject)
    {
        ObDereferenceObject(kbdDriverObject);
        kbdDriverObject = NULL;
    }

    return Status;
}