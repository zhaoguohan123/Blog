#include "ProcessTime.h"
#include "Comm.h"
#include "util.h"
#define CACHE_REFRESH_TIME 300

typedef VOID(*PFN_PROCESS_NOTIFY_CALLBACK)(PEPROCESS, HANDLE, PPS_CREATE_NOTIFY_INFO);
PFN_PROCESS_NOTIFY_CALLBACK processNotifyCallBack = NULL;
PROCESS_TIME_RECORD record = { 0 };
ERESOURCE recordRes;

// ------------------
// 
// ֪ͨ��������ȡrecord
NTSTATUS GetTimeRecordBeNotify(OUT PPROCESS_TIME_RECORD desRecord)
{
    NTSTATUS stat = STATUS_UNSUCCESSFUL;
    if (desRecord != NULL && MmIsAddressValid(desRecord))
    {
        __try
        {
            ExAcquireResourceSharedLite(&recordRes, TRUE);
            RtlCopyMemory(desRecord, &record, sizeof(PROCESS_TIME_RECORD));
            ExReleaseResourceLite(&recordRes);

            stat = STATUS_SUCCESS;
        }
        __except (1)
        {
            KdPrint(("[error]:ProcessTime_GetTimeRecordBeNotify __except !! \r\n"));
            stat = STATUS_UNSUCCESSFUL;
        }
    }
    return stat;
}

// ����pid��ȡrecord,��֤��Ч�ͻ�ȡ��ǰʱ��
// �����в��洢ʱ����Ч�Ժ͵�ǰʱ��,��Ч��ֻ�ڴ˺����ͽ��̽���֪ͨ��������֤,����ǰʱ��ֻ�ڴ˺������л�ȡ
NTSTATUS GetSingleProcessRunTime(IN HANDLE pid, OUT PPROCESS_TIME_RECORD record)
{
    NTSTATUS stat = STATUS_UNSUCCESSFUL;
    LARGE_INTEGER sysCurTime = { 0 };
    LARGE_INTEGER currentTime = { 0 };
    if (pid)
    {
        KeQuerySystemTime(&sysCurTime);
        ExSystemTimeToLocalTime(&sysCurTime, &currentTime);

        ExAcquireResourceExclusiveLite(&recordRes, TRUE);
        stat = GetTimeRecordByHandle(pid, record, sizeof(PROCESS_TIME_RECORD));
        if (currentTime.QuadPart < record->startTime.QuadPart || (record->endFlag && currentTime.QuadPart > record->endTime.QuadPart))
        {
            record->isValid = FALSE;
        }
        record->curTime = currentTime;
        ExReleaseResourceLite(&recordRes);
    }
    return stat;
}
// 
// ------------------

// ���»���
VOID UpdataTimeRecordCache()
{
    // ���£��н���ʱ����£�ˢ�»����ʱ���ڳ�����ͬPID���£��µ�recordֱ�����
    ExAcquireResourceExclusiveLite(&recordRes, TRUE);
    RtlDeleteRecordToList(&record);
    RtlAddRecordToList(&record, TRUE);
    ExReleaseResourceLite(&recordRes);

    // ˢ�»���: ɾ�������н���ʱ�䳬��x��ļ�¼
    LARGE_INTEGER sysCurTime = { 0 };
    LARGE_INTEGER currentTime = { 0 };
    KeQuerySystemTime(&sysCurTime);
    ExSystemTimeToLocalTime(&sysCurTime, &currentTime);
    RefreshRecordList(currentTime, CACHE_REFRESH_TIME);
    
}

// ������
VOID UnInitTimeRecordCache()
{
    RtlUnInitTimeRecordList();
}

// ��ʼ������
VOID InitTimeRecordCache()
{
    RtlInitTimeRecordList();

    NTSTATUS status;
    ULONG returnLength;
    PVOID buffer = NULL;
    PSYSTEM_PROCESS_INFORMATION current = NULL;

    status = ZwQuerySystemInformation(SystemProcessInformation, buffer, 0, &returnLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH)
    {
        buffer = ExAllocatePool(NonPagedPool, returnLength);
        if (buffer)
        {
            RtlZeroMemory(buffer, returnLength);
            status = ZwQuerySystemInformation(SystemProcessInformation, buffer, returnLength, &returnLength);
            if (NT_SUCCESS(status))
            {
                current = (PSYSTEM_PROCESS_INFORMATION)buffer;
                while (current->NextEntryOffset != 0)
                {
                    PROCESS_TIME_RECORD timeRecord = { 0 };
                    LARGE_INTEGER localTime = { 0 };
                    ExSystemTimeToLocalTime(&(current->CreateTime), &localTime);

                    timeRecord.isValid = TRUE;
                    timeRecord.endFlag = FALSE;
                    timeRecord.processID = current->UniqueProcessId;
                    timeRecord.startTime = localTime;
                    RtlAddRecordToList(&timeRecord, TRUE);

                    current = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)current + current->NextEntryOffset);
                };
            }
            ExFreePool(buffer);
        }
    }
    if (!NT_SUCCESS(status))
    {
        KdPrint(("[error]:InitTimeRecordCache error -- error code is %x\r\n", status));
    }
}

// ���̴���������֪ͨ�ص�
VOID ProcessNotifyCallback(PEPROCESS process, HANDLE processId, PPS_CREATE_NOTIFY_INFO create)
{
    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return;
    }

    LARGE_INTEGER currentTime = { 0 };
    LARGE_INTEGER localTime = { 0 };
    KeQuerySystemTime(&currentTime);
    ExSystemTimeToLocalTime(&currentTime, &localTime);

    ExAcquireResourceExclusiveLite(&recordRes, TRUE);
    if (create != NULL)
    {
        //KdPrint(("[info]:ProcessTime_ProcessNotifyCallback -- Process with ID %llu has create.\n", (ULONG64)processId));
        record.isValid = TRUE;
        record.endFlag = FALSE;
        record.processID = processId;
        record.startTime = localTime;   // ��ʼʱ��
        record.endTime.QuadPart = 0;
    }
    else
    {
        //KdPrint(("[info]:ProcessTime_ProcessNotifyCallback -- Process with ID %llu has exited.\n", (ULONG64)processId));
        record.endFlag = TRUE;
        record.processID = processId;
        record.endTime = localTime;     // ����ʱ��
        record.isValid = record.endTime.QuadPart < record.startTime.QuadPart ? FALSE : TRUE;
    }
    ExReleaseResourceLite(&recordRes);

    UpdataTimeRecordCache();
    NotifyRecordTime();
    WaitNotifyRecordTime(200);
}

// ɾ��
VOID UnInitProcessTimeRecorder(PDRIVER_OBJECT pDrv)
{
    ExDeleteResourceLite(&recordRes);

    UnInitTimeRecordCache();

    if (processNotifyCallBack != NULL)
    {
        ULONG oldFlags = RtlFakeCallBackVerify(pDrv->DriverSection);
        PsSetCreateProcessNotifyRoutineEx(processNotifyCallBack, TRUE);
        RtlResetCallBackVerify(pDrv->DriverSection, oldFlags);
    }
    processNotifyCallBack = NULL;
}

// ��ʼ��
NTSTATUS InitProcessTimeRecorder(PDRIVER_OBJECT pDrv)
{
    NTSTATUS status = STATUS_SUCCESS;

    ExInitializeResourceLite(&recordRes);

    InitTimeRecordCache();

    processNotifyCallBack = ProcessNotifyCallback;

    ULONG oldFlags = RtlFakeCallBackVerify(pDrv->DriverSection);
    status = PsSetCreateProcessNotifyRoutineEx(processNotifyCallBack, FALSE);
    if (NT_SUCCESS(status))
    {
        KdPrint(("[info]:ProcessTime_InitRecordProcessTime Success !!\n"));
    }
    RtlResetCallBackVerify(pDrv->DriverSection, oldFlags);

    return status;
}