
#include "global.h"
#include "kbd.h"

PDEVICE_OBJECT gPocDeviceObject = NULL;
PKEVENT g_pEvent = NULL;
HANDLE g_hEvent = NULL;

extern BOOLEAN g_DisableCad;
extern BOOLEAN g_DisableKeyBoard;

HANDLE g_hPipeClient;
IO_STATUS_BLOCK g_ioStatusBlock;
KEVENT g_PipeEvent;

NTSTATUS
PocDeviceCreate(
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
PocDeviceClose(
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
PocDeviceControlOperation(
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

    listEntry = ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

    while (listEntry != &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead)
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
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->KbdObj is null.\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        goto EXIT;
    }

    ULONG major_code = IrpSp->MajorFunction;
    ULONG contol_code = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    if (major_code == IRP_MJ_DEVICE_CONTROL)
    {
        switch (contol_code)
        {
        case IOCTL_CODE_TO_DISABLE_ALL_KEY:
            DbgPrint("r3 notify disable kbd!");
            g_DisableKeyBoard = TRUE;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;

        case IOCTL_CODE_TO_ENABLE_ALL_KEY:
            DbgPrint("r3 notify enable kbd!");
            g_DisableKeyBoard = FALSE;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;

        case IOCTL_CODE_TO_ENABLE_CAD:
            DbgPrint("r3 notify enable cad!");
            g_DisableCad = FALSE;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;

        case IOCTL_CODE_TO_DISABLE_CAD:
            DbgPrint("r3 notify disable cad!");
            g_DisableCad = TRUE;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;

        case IOCTL_CODE_TO_CREATE_EVENT:
            /*
            *  创建和客户端通信的事件
            */
            DbgPrint("r3 notify create cad event!");
            if (g_pEvent != NULL)
            {
                DbgPrint("g_pEvent is not NULL, event has been exits!");
                goto EXIT;
            }

            g_pEvent = (PKEVENT)ExAllocatePool(NonPagedPool, sizeof(KEVENT));
            UNICODE_STRING ustrEventName = RTL_CONSTANT_STRING(L"\\BaseNamedObjects\\Kbd_fltr");
            g_pEvent = IoCreateNotificationEvent(&ustrEventName, &g_hEvent);
            if (g_pEvent == NULL)
            {
                DbgPrint("IoCreateSynchronizationEvent failed!");
                goto EXIT;
            }

            // 事件初始化为非触发状态
            (void)KeInitializeEvent(&g_hEvent, NotificationEvent, FALSE);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        default:
            break;
        }
    }

    IoSkipCurrentIrpStackLocation(Irp);

    Status = IoCallDriver(KbdObj->KbdDeviceObject, Irp);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoCallDriver failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
    }

EXIT:

    return Status;
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

    listEntry = ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

    while (listEntry != &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead)
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
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->KbdObj is null.\n", __FUNCTION__));
        goto EXIT;
    }

    /*
    * Irp内存，KbdDeviceObject内存有可能已经被释放了，设备也被移除了，使用这个标识防止Pagefault
    */
    ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

    KbdObj->IrpCancel = TRUE;
    KbdObj->KbdFileObject->DeviceObject = KbdObj->BttmDeviceObject;

    ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);

    /*
    * 通常发生在键盘卸载时，IoCancelIrp(Irp)，Poc驱动IoCancelIrp(NewIrp)，这样Kbdclass里的NewIrp也会返回
    */
    if (NULL != KbdObj->NewIrp)
    {
        IoCancelIrp(KbdObj->NewIrp);
    }

EXIT:

    Irp->IoStatus.Status = STATUS_CANCELLED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}


VOID
PocHandleReadThread(
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

    listEntry = ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

    while (listEntry != &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead)
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
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->KbdObj is null.\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        goto EXIT;
    }

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("\n read complete %s->ExInterlockedInsertTailList gPocDeviceObject = %p KbdObj = %p KbdDeviceObject = %p.\nBttmDeviceObject = %p KbdFileObject = %p.\n",
            __FUNCTION__,
            gPocDeviceObject,
            KbdObj,
            KbdObj->KbdDeviceObject,
            KbdObj->BttmDeviceObject,
            KbdObj->KbdFileObject));


    RemoveLock = (PIO_REMOVE_LOCK)((PCHAR)KbdObj->KbdDeviceObject->DeviceExtension + REMOVE_LOCK_OFFET_DE);

    if (Irp == KbdObj->RemoveLockIrp)
    {
        IoReleaseRemoveLock(RemoveLock, Irp);
        KbdObj->RemoveLockIrp = NULL;
    }

    // 构造IRP
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
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoBuildSynchronousFsdRequest NewIrp failed..\n", __FUNCTION__));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }


    KeClearEvent(NewIrp->UserEvent);                                //UserEvent当IRP请求结束的时候，该事件会被触发
    NewIrp->Tail.Overlay.Thread = PsGetCurrentThread();
    NewIrp->Tail.Overlay.AuxiliaryBuffer = NULL;
    NewIrp->RequestorMode = KernelMode;
    NewIrp->PendingReturned = FALSE;
    NewIrp->Cancel = FALSE;
    NewIrp->CancelRoutine = NULL;

    /*
    * UserApcRoutine == win32kbase!rimInputApc，与NT4的Win32k源码不同，Windows 10使用的是APC而非Event,
    */
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
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoCallDriver failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
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
            PocPrintScanCode(InputData);

            PocConfigureKeyMapping(InputData);
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
PocReadOperation(
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

        listEntry = ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

        while (listEntry != &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead)
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
            PocHandleReadThread,
            Irp);
        
        if (!NT_SUCCESS(Status))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->PsCreateSystemThread PocHandleReadThread failed. Status = 0x%x.\n",
                    __FUNCTION__,
                    Status));
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
PocIrpHookInitThread(
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
    * 100ms定时扫描是否有新键盘设备
    */
    while (!((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gIsUnloading)
    {
        KbdDeviceObject = ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdDriverObject->DeviceObject;

        /*
        * 判断键盘设备是否已经加到链表中了; 
        */
        while (NULL != KbdDeviceObject)
        {
            listEntry = ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead.Flink;

            while (listEntry != &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead)
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
            * 开始键盘Hook的初始化工作
            */
            KbdDeviceExtension = KbdDeviceObject->DeviceExtension;
            RemoveLock = (PIO_REMOVE_LOCK)(KbdDeviceExtension + REMOVE_LOCK_OFFET_DE);
            SpinLock = (PKSPIN_LOCK)(KbdDeviceExtension + SPIN_LOCK_OFFSET_DE);

            /*
            * 从Kbdclass的IRP链表DeviceExtension->ReadQueue取出IRP
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
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->ExAllocatePoolWithTag KbdObj failed.\n",
                        __FUNCTION__));
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto EXIT;
            }

            RtlZeroMemory(KbdObj, sizeof(POC_KBDCLASS_OBJECT));

            /*
            * KbdDeviceObject是Kbdclass为每个底层设备分配的的设备对象，
            * gBttmDeviceObject是Kbdclass设备栈最低层的设备对象，通常为PS/2，USB HID键盘
            */
            KbdObj->SafeUnload = FALSE;
            KbdObj->RemoveLockIrp = Irp;
            KbdObj->KbdFileObject = IrpSp->FileObject;
            KbdObj->BttmDeviceObject = IrpSp->FileObject->DeviceObject;
            KbdObj->KbdDeviceObject = KbdDeviceObject;

            KeInitializeEvent(&KbdObj->Event, SynchronizationEvent, FALSE);
            ExInitializeResourceLite(&KbdObj->Resource);

            /*
            * 替换FileObject->DeviceObject为gPocDeviceObject，
            * 这样Win32k的IRP就会发到我们的Poc驱动
            */
            KbdObj->KbdFileObject->DeviceObject = gPocDeviceObject;

            gPocDeviceObject->StackSize = max(KbdObj->BttmDeviceObject->StackSize, gPocDeviceObject->StackSize);


            ExInterlockedInsertTailList(
                &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead,
                &KbdObj->ListEntry,
                &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjSpinLock);

            KbdObj->InitSuccess = TRUE;

            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("\n%s->ExInterlockedInsertTailList gPocDeviceObject = %p KbdObj = %p KbdDeviceObject = %p.\nBttmDeviceObject = %p KbdFileObject = %p.\n",
                    __FUNCTION__,
                    gPocDeviceObject,
                    KbdObj,
                    KbdObj->KbdDeviceObject,
                    KbdObj->BttmDeviceObject,
                    KbdObj->KbdFileObject));

            Status = PsCreateSystemThread(
                &ThreadHandle,
                THREAD_ALL_ACCESS,
                NULL,
                NULL,
                NULL,
                PocHandleReadThread,
                Irp);

            if (!NT_SUCCESS(Status))
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->PsCreateSystemThread PocHandleReadThread failed. Status = 0x%x.\n",
                        __FUNCTION__,
                        Status));
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


VOID
PocKbdObjListCleanup(
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

    while (!IsListEmpty(&((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead))
    {
        listEntry = ExInterlockedRemoveHeadList(
            &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead,
            &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjSpinLock);

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
            * 这个IRP是Poc驱动PocHandleReadThread函数发给Kbdclass的，
            * IoCompleteRequest以后，PocHandleReadThread的KeWaitForSingleObject会结束等待，PocHandleReadThread线程也会退出
            */
            Irp->IoStatus.Status = 0;
            Irp->IoStatus.Information = 0;

            IoReleaseRemoveLock(RemoveLock, Irp);
            IoCompleteRequest(Irp, IO_NO_INCREMENT);


            while (!KbdObj->SafeUnload)
            {
                KeDelayExecutionThread(KernelMode, FALSE, &Interval);
            }


            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("\n%s->Safe to unload. gPocDeviceObject = %p KbdObj = %p KbdDeviceObject = %p.\nBttmDeviceObject = %p KbdFileObject = %p KbdFileObject->DeviceObject = %p.\n",
                    __FUNCTION__,
                    gPocDeviceObject,
                    KbdObj,
                    KbdObj->KbdDeviceObject,
                    KbdObj->BttmDeviceObject,
                    KbdObj->KbdFileObject,
                    KbdObj->KbdFileObject->DeviceObject));
        }
        else
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->Device has been removed. gPocDeviceObject = %p KbdObj = %p.\n\n",
                    __FUNCTION__, gPocDeviceObject, KbdObj));
        }
        

        ExDeleteResourceLite(&KbdObj->Resource);

        if (NULL != KbdObj)
        {
            ExFreePoolWithTag(KbdObj, POC_NONPAGED_POOL_TAG);
            KbdObj = NULL;
        }
    }

    /*
    * 等PocIrpHookInitThread退出
    */
    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}


VOID 
PocUnload(
    _In_ PDRIVER_OBJECT  DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gIsUnloading = TRUE;

    PocKbdObjListCleanup();


    if (NULL != ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdDriverObject)
    {
        ObDereferenceObject(((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdDriverObject);
        ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdDriverObject = NULL;
    }

    if (g_pEvent != NULL)
    {
        (void)KeClearEvent(g_pEvent);

        (void)ZwClose(g_hEvent);

        g_pEvent = NULL;
        g_hEvent = NULL;
    }

    //删除符号设备
    UNICODE_STRING kbd_syb = RTL_CONSTANT_STRING(CDO_SYB_NAME);
    if (NT_SUCCESS(IoDeleteSymbolicLink(&kbd_syb)))
    {
        DbgPrint("IoDeleteSymbolicLink failed. \n");
    }

    if (NULL != gPocDeviceObject)
    {
        IoDeleteDevice(gPocDeviceObject);
        gPocDeviceObject = NULL;
    }
}


// 初始化管道
void initpipe()
{
    UNICODE_STRING uniName;
    OBJECT_ATTRIBUTES objAttr;

    RtlInitUnicodeString(&uniName, L"\\DosDevices\\Pipe\\LySharkPipeConn");
    InitializeObjectAttributes(&objAttr, &uniName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    ZwCreateFile(&g_hPipeClient, GENERIC_READ | GENERIC_WRITE, &objAttr, &g_ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    if (!g_hPipeClient)
    {
        return;
    }
    KeInitializeEvent(&g_PipeEvent, SynchronizationEvent, TRUE);
}

// 将数据传到R3应用层
VOID ReportToClient(char* m_parameter, int lent)
{
    if (!NT_SUCCESS(ZwWriteFile(g_hPipeClient, NULL, NULL, NULL, &g_ioStatusBlock, (void*)m_parameter, lent, NULL, NULL)))
    {
        DbgPrint("写出错误");
    }
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
    PDRIVER_OBJECT KbdDriverObject = NULL;

    HANDLE ThreadHandle = NULL;

    RtlInitUnicodeString(&DriverName, KBD_DEV_NAME);

    Status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &DriverName,
        FILE_DEVICE_KEYBOARD,
        0,
        FALSE,
        &gPocDeviceObject);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoCreateDevice failed. Status = 0x%x.\n",
                __FUNCTION__, Status));
        goto EXIT;
    }

    /*
    * 使用内存Irp->AssociatedIrp.SystemBuffer
    */
    gPocDeviceObject->Flags |= DO_BUFFERED_IO;


    /*
    * 找到键盘驱动Kbdclass的驱动对象
    */
    RtlInitUnicodeString(&DriverName, L"\\Driver\\Kbdclass");

    Status = ObReferenceObjectByName(
        &DriverName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        FILE_ALL_ACCESS,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        &KbdDriverObject);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ObReferenceObjectByName %ws failed. Status = 0x%x.\n",
                __FUNCTION__,
                DriverName.Buffer,
                Status));
        goto EXIT;
    }


    ((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdDriverObject = KbdDriverObject;

    InitializeListHead(&((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjListHead);

    KeInitializeSpinLock(&((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjSpinLock);

    // 创建驱动的符号链接
    UNICODE_STRING kbd_dev_name = RTL_CONSTANT_STRING(KBD_DEV_NAME);
    UNICODE_STRING kbd_syb = RTL_CONSTANT_STRING(CDO_SYB_NAME);

    // 先删除
    Status = IoCreateSymbolicLink(&kbd_syb, &kbd_dev_name);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("IoCreateSymbolicLink failed. \n");
        goto EXIT;
    }

    DriverObject->MajorFunction[IRP_MJ_READ] = PocReadOperation;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PocDeviceControlOperation;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = PocDeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = PocDeviceClose;

    DriverObject->DriverUnload = PocUnload;
    
    initpipe();
    /*
    * 创建键盘Hook初始化线程
    */
    Status = PsCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        PocIrpHookInitThread,
        NULL);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PsCreateSystemThread PocIrpHookInitThread failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
        goto EXIT;
    }

    if (NULL != ThreadHandle)
    {
        ZwClose(ThreadHandle);
        ThreadHandle = NULL;
    }


    Status = STATUS_SUCCESS;

EXIT:

    if (!NT_SUCCESS(Status) && NULL != gPocDeviceObject)
    {
        IoDeleteDevice(gPocDeviceObject);
        gPocDeviceObject = NULL;
    }

    if (!NT_SUCCESS(Status) && NULL != KbdDriverObject)
    {
        ObDereferenceObject(KbdDriverObject);
        KbdDriverObject = NULL;
    }

    return Status;
}
