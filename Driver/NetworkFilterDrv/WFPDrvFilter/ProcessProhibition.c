#include "Comm.h"
#include "util.h"
#include "ProcessProhibition.h"
#include "InjectList.h"
#include "ProcessInjection.h"

PROCESS_CREATE_INFO createPorcessInfo = { 0 };
PROCESS_RUNTIME_STAT processRuntimeStat = { 0 };
ERESOURCE infoRes;

BOOLEAN IsProcessInject()
{
	BOOLEAN res = processRuntimeStat.isInject;
	return res;
}

// ֪ͨ��������ȡPorcessInfo
NTSTATUS GetCreateProcessInfoBeNotify(OUT PPROCESS_CREATE_INFO desProcessInfo)
{
	NTSTATUS stat = STATUS_UNSUCCESSFUL;
	if (desProcessInfo != NULL && MmIsAddressValid(desProcessInfo))
	{
		__try
		{
			RtlCopyMemory(desProcessInfo, &createPorcessInfo, sizeof(PROCESS_CREATE_INFO));
			stat = STATUS_SUCCESS;
		}
		__except (1)
		{
			KdPrint(("[error]:ProcessProhibition_GetCreateProcessInfoBeNotify __except !! \r\n"));
			stat = STATUS_UNSUCCESSFUL;
		}
	}
	return stat;
}

// ���ý��̽�����Ϣ
NTSTATUS SetProcessRuntimeStat(PPROCESS_RUNTIME_STAT runStat)
{
	if (runStat != NULL && MmIsAddressValid(runStat))
	{
		RtlCopyMemory(&processRuntimeStat, runStat, sizeof(PROCESS_RUNTIME_STAT));
		return STATUS_SUCCESS;
	}
	return STATUS_UNSUCCESSFUL;
}

// ���ý�����Ϣ
NTSTATUS SetUserProcess(PPS_CREATE_NOTIFY_INFO info)
{
	UNICODE_STRING outName = { 0 };
	UNICODE_STRING objectFileName = { 0 };
	UNICODE_STRING dosName = { 0 };
	UNICODE_STRING outStr;
	WCHAR desBuf[MAX_PATH];
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	RtlInitEmptyUnicodeString(&outStr, desBuf, MAX_PATH * sizeof(WCHAR));
	RtlInitUnicodeString(&objectFileName, info->FileObject->FileName.Buffer);
	status = IoVolumeDeviceToDosName(info->FileObject->DeviceObject, &dosName);
	if (NT_SUCCESS(status))
	{
		status = RtlAppendUnicodeStringToString(&outStr, &dosName);
		if (NT_SUCCESS(status))
		{
			status = RtlAppendUnicodeStringToString(&outStr, &objectFileName);
			if (NT_SUCCESS(status))
			{
				RtlCopyMemory(createPorcessInfo.filePath, outStr.Buffer, outStr.Length);
				status = RtlGetFileName(&outStr, &outName);
				if (NT_SUCCESS(status))
				{
					if (outName.Buffer != NULL && MmIsAddressValid(outName.Buffer))
					{
						RtlCopyMemory(createPorcessInfo.fileName, outName.Buffer, outName.Length);
					}
				}
			}
		}
	}
	return status;
}

// ���ý���������ֹ����
VOID CbCreateProcess(_Inout_ PEPROCESS process, _In_ HANDLE processId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO createInfo)
{
    if (processId == (HANDLE)4 || processId == (HANDLE)0)
    {
        return;
    }
    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return;
    }
    if (createInfo != NULL)
    {
		RtlZeroMemory(&processRuntimeStat, sizeof(PROCESS_RUNTIME_STAT));	// ÿ�����̶����ʼ��
		RtlZeroMemory(&createPorcessInfo, sizeof(PROCESS_CREATE_INFO));

		// �� 
		ExAcquireResourceExclusiveLite(&infoRes, TRUE);
		if (NT_SUCCESS(SetUserProcess(createInfo)))
		{
			createPorcessInfo.processId = processId;
			createPorcessInfo.parentProcessId = createInfo->ParentProcessId;
		}
		NotifyProcessProhibition();
		WaitNotifyProcessProhibition(2000);
		if (processRuntimeStat.isProhibitCreate)
		{
			createInfo->CreationStatus = STATUS_ACCESS_DENIED;
		}
		// ע���������
		if (processRuntimeStat.isInject == TRUE)
		{
			RtlAddInjectList(processId);
		}
		ExReleaseResourceLite(&infoRes);
    }
	else
	{
		// ע������ɾ��
		RtlDeleteInjectList(processId);
	}
}

// ж��
VOID UnInitProcessProhibition(PDRIVER_OBJECT pDrv)
{
	RtlUnInitInjectList();

	ExDeleteResourceLite(&infoRes);

    ULONG oldFlags = RtlFakeCallBackVerify(pDrv->DriverSection);

    PsSetCreateProcessNotifyRoutineEx(CbCreateProcess, TRUE);

    RtlResetCallBackVerify(pDrv->DriverSection, oldFlags);

	UnInitProcessInjection(pDrv);
}

// ���̽����ʼ��
NTSTATUS InitProcessProhibition(PDRIVER_OBJECT pDrv)
{
    NTSTATUS stat = STATUS_SUCCESS;

	RtlInitInjectList();

	ExInitializeResourceLite(&infoRes);

    ULONG oldFlags = RtlFakeCallBackVerify(pDrv->DriverSection);

    stat = PsSetCreateProcessNotifyRoutineEx(CbCreateProcess, FALSE);

    RtlResetCallBackVerify(pDrv->DriverSection, oldFlags);

	stat = InitProcessInjection(pDrv);	//  ����ע��

    return stat;
}
