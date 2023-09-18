#include "worker.h"

#define LOG_FILE_NAME_W			L"\\??\\C:\\svoldrvLog.txt"
#define STR_BUF_SIZE			512

PDEVICE_OBJECT		filterDevice;

NTSTATUS SnapWriteLog(PVOID pWorkerParamBlock);

SNAP_WORKER_BEGIN_MAP(g_SnapWorkerTable)
	ON_SNAP_WORKER_MAP(SNAP_WORKER_WRITE_LOG, SnapWriteLog)
SNAP_WORKER_END_MAP()

/* Should be reentered */
char * GetTimeStampString()
{
#if 0
	static BOOLEAN bInitialzed = FALSE;
	static LARGE_INTEGER	StartingTime;
	static LARGE_INTEGER	Frequency;
	static LARGE_INTEGER	EndingTime, ElapsedMicroseconds;
	static char				Buffer[512];

	if (!bInitialzed)
	{
		StartingTime = KeQueryPerformanceCounter(&Frequency);
		bInitialzed = TRUE;
	}

	EndingTime = KeQueryPerformanceCounter(NULL);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
	ElapsedMicroseconds.QuadPart *= 1000000;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;
	sprintf_s(Buffer, sizeof(Buffer), "%013d", ElapsedMicroseconds.QuadPart);
	return Buffer;
#else
	char * p_time_str = NULL;
	LARGE_INTEGER sys_tm;
	static LARGE_INTEGER sys_tm_prev = { 0 };
	LARGE_INTEGER loc_tm;
	TIME_FIELDS	 tm_fields;

	p_time_str = (char *)ExAllocatePoolWithTag(NonPagedPool, 100, 'rdvS');
	if (!p_time_str)
		return NULL;

	p_time_str[0] = '\0';
	KeQuerySystemTime(&sys_tm);
	ExSystemTimeToLocalTime(&sys_tm, &loc_tm);
	RtlTimeToTimeFields(&loc_tm, &tm_fields);

	sprintf_s(p_time_str, 100, "%02d:%02d:%02d-%03dms, %d, %d",
		tm_fields.Hour, tm_fields.Minute, tm_fields.Second, tm_fields.Milliseconds,
		sys_tm.u.HighPart - sys_tm_prev.u.HighPart, sys_tm.u.LowPart - sys_tm_prev.u.LowPart);

	sys_tm_prev = sys_tm;

	return p_time_str;
#endif
}

/* Should be called at PASSIVE_LEVEL because
ZwCreateFile runs at PASSIVE_LEVEL
*/
NTSTATUS
AppendDataToFileSynchronously(
	IN const PWCHAR uFileName,
	IN char* Buffer,
	IN size_t Size
	)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	OBJECT_ATTRIBUTES	oaFile;
	UNICODE_STRING		usFile;
	HANDLE				hFile = NULL;
	IO_STATUS_BLOCK		iosb;
	KIRQL				curIrql;

	curIrql = KeGetCurrentIrql();

	if (PASSIVE_LEVEL != curIrql)
	{
		Status = STATUS_UNSUCCESSFUL;
		goto ERROR_EXIT;
	}

	RtlInitUnicodeString(&usFile, uFileName);

	// Create File
	InitializeObjectAttributes(
		&oaFile, &usFile,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);
	Status = ZwCreateFile(
		&hFile,
		SYNCHRONIZE | FILE_APPEND_DATA,	// Write Synchronously
		&oaFile,
		&iosb,
		NULL, FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_OPEN_IF,
		FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);
	if (!NT_SUCCESS(Status)) {
		goto ERROR_EXIT;
	}

	// Output
	Status = ZwWriteFile(hFile,
		NULL,
		NULL, NULL,		// Reserved
		&iosb,			// How much bytes are transferred
		Buffer, (ULONG)Size,	// Buffer and Size
		NULL,			// Set ByteOffset to NULL means end of the file
		NULL			// Device and intermediate drivers should set this pointer to NULL
		);

ERROR_EXIT:
	// Close
	if (hFile)
		ZwClose(hFile);

	return Status;
}

VOID FLTAPI FltMainWorkerRoutine(PFLT_GENERIC_WORKITEM FltWorkItem, PFLT_FILTER Filter, PVOID Context)
{

	PSNAP_WORKER_CONTEXT pWorkerContext = (PSNAP_WORKER_CONTEXT)Context;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	
	//ntStatus = PsImpersonateClient(KeGetCurrentThread(), pWorkerContext->AccessToken, TRUE, TRUE, SecurityImpersonation);

	if (!NT_SUCCESS(ntStatus))
	{
		//分发表
		for (ULONG uIndex = 0; uIndex < _countof(g_SnapWorkerTable); ++uIndex)
		{
			if (pWorkerContext->WorkerType == g_SnapWorkerTable[uIndex].WorkerType &&
				g_SnapWorkerTable[uIndex].pFun != NULL)
			{
				pWorkerContext->Status = g_SnapWorkerTable[uIndex].pFun(pWorkerContext->WorkerParamBlock);
				break;
			}
		}

		//PsRevertToSelf();
	}
	else
	{
		pWorkerContext->Status = ntStatus;
	}

	if (pWorkerContext->WaitComplete)
	{
		//完成分发
		KeSetEvent(&pWorkerContext->CompleteEvent, IO_NO_INCREMENT, FALSE);
	}
	else
	{
		//不等待完成的，需要释放内存, 因为异步，外部不会释放内存
		ExFreePoolWithTag(pWorkerContext->WorkerParamBlock, 'rdvS');
		ExFreePoolWithTag(pWorkerContext, 'rdvS');
	}
	if (FltWorkItem != NULL)
	{
		FltFreeGenericWorkItem(FltWorkItem);
		FltWorkItem = NULL;
	}
}

//WorkerParamBlock 参数默认放在pool中，函数会释放这个内存，外部不能再使用，无论成功或者失败
NTSTATUS DispatchSnapWorkerRoutine(PVOID  FltObject, SNAP_WORKER_TYPE WorkerTyper, PVOID WorkerParamBlock, BOOLEAN Wait)
{
	PSNAP_WORKER_CONTEXT pSnapWorkerContext = NULL;
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PFLT_GENERIC_WORKITEM FltWorkItem = NULL;

	PACCESS_TOKEN pAccessToken = NULL;
	BOOLEAN CopyOnOpen = FALSE;
	BOOLEAN EffectiveOnly = FALSE;
	SECURITY_IMPERSONATION_LEVEL Sil = SecurityAnonymous;

	pSnapWorkerContext = (PSNAP_WORKER_CONTEXT)ExAllocatePoolWithTag(PagedPool, sizeof (SNAP_WORKER_CONTEXT), 'rdvS');
	if (pSnapWorkerContext == NULL)
	{
		ntStatus = STATUS_NO_MEMORY;
		goto FailCleanup;
	}

	/*
	pAccessToken = PsReferenceImpersonationToken(KeGetCurrentThread(), &CopyOnOpen, &EffectiveOnly, & Sil);
	if (pSnapWorkerContext->AccessToken == NULL)
	{
		pSnapWorkerContext->AccessToken = PsReferencePrimaryToken(PsGetCurrentProcess());
	}
	else
	{
		pSnapWorkerContext->AccessToken = pAccessToken;
	}
	*/
	pSnapWorkerContext->WorkerType = WorkerTyper;
	pSnapWorkerContext->WorkerParamBlock = WorkerParamBlock;
	KeInitializeEvent(&pSnapWorkerContext->CompleteEvent, NotificationEvent, FALSE);
	pSnapWorkerContext->WaitComplete = Wait;
	pSnapWorkerContext->Status = STATUS_UNSUCCESSFUL;

	//开始分发
	FltWorkItem = FltAllocateGenericWorkItem();
	if (FltWorkItem == NULL)
	{
		ntStatus = STATUS_NO_MEMORY;
		goto FailCleanup;
	}

	ntStatus = FltQueueGenericWorkItem(FltWorkItem, FltObject, (PFLT_GENERIC_WORKITEM_ROUTINE)FltMainWorkerRoutine, CriticalWorkQueue, (PVOID)pSnapWorkerContext);
	if (!NT_SUCCESS(ntStatus))
	{
		goto FailCleanup;
	}

	//等待的函数，自己释放内存, 非等待的，交给workitem释放
	if (Wait)
	{
		KeWaitForSingleObject(&pSnapWorkerContext->CompleteEvent, Executive, KernelMode, FALSE, NULL);

		/*
		if (pAccessToken != NULL)
		{
			PsDereferencePrimaryToken(pAccessToken);
			pAccessToken = NULL;
		}
		else if (pSnapWorkerContext->AccessToken != NULL)
		{
			PsDereferenceImpersonationToken(pSnapWorkerContext->AccessToken);
			pSnapWorkerContext->AccessToken = NULL;
		}*/

		ntStatus = pSnapWorkerContext->Status;
		ExFreePoolWithTag(WorkerParamBlock, 'rdvS');
		ExFreePoolWithTag(pSnapWorkerContext, 'rdvS');
	}

	return ntStatus;

FailCleanup:

	if (FltWorkItem != NULL)
	{
		FltFreeGenericWorkItem(FltWorkItem);
		FltWorkItem = NULL;
	}

	if (pSnapWorkerContext != NULL)
	{

		if (pAccessToken != NULL)
		{
			PsDereferencePrimaryToken(pAccessToken);
			pAccessToken = NULL;
		}
		else if (pSnapWorkerContext->AccessToken != NULL)
		{
			PsDereferenceImpersonationToken(pSnapWorkerContext->AccessToken);
			pSnapWorkerContext->AccessToken = NULL;
		}
		ExFreePoolWithTag(pSnapWorkerContext, 'rdvS');
		pSnapWorkerContext = NULL;
	}

	if (WorkerParamBlock != NULL)
	{
		ExFreePoolWithTag(WorkerParamBlock, 'rdvS');
		WorkerParamBlock = NULL;
	}

	return ntStatus;
}

NTSTATUS SnapWriteLog(PVOID pWorkerParamBlock)
{
	char* pLog = (char *)pWorkerParamBlock;

	return AppendDataToFileSynchronously(LOG_FILE_NAME_W, pLog, strlen(pLog) * sizeof(char));
}

NTSTATUS
WriteLogLnWithTimeStampInWorkItem(
	IN char * fmt,
	IN ...)
{
	va_list		arg_list;
	ULONG		OpId;
	NTSTATUS	Status = STATUS_SUCCESS;
	char *pBuf1 = NULL, *pBuf2 = NULL;
	char *p_ts_str = NULL;

	/* Allocate Memory */
	p_ts_str = GetTimeStampString();
	pBuf1 = (char *)ExAllocatePoolWithTag(NonPagedPool, STR_BUF_SIZE, 'rdvS');
	pBuf2 = (char *)ExAllocatePoolWithTag(NonPagedPool, STR_BUF_SIZE, 'rdvS');

	if (!pBuf1 || !pBuf2 || !p_ts_str) {
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto ERROR_EXIT;
	}

	/* Generate Buffer */
	va_start(arg_list, fmt);
	vsprintf_s(pBuf2, STR_BUF_SIZE - 1, fmt, arg_list);
	va_end(arg_list);

	sprintf_s(pBuf1, STR_BUF_SIZE - 1, "%s : %s\r\n",
		p_ts_str, pBuf2);

	// buf1 DispatchSnapWorkerRoutine函数里面会释放
	return DispatchSnapWorkerRoutine(filterDevice, SNAP_WORKER_WRITE_LOG, pBuf1, TRUE);

ERROR_EXIT:

	if (pBuf2) {
		ExFreePoolWithTag(pBuf2, 'rdvS');
		pBuf2 = NULL;
	}

	if (p_ts_str) {
		ExFreePool(p_ts_str);
		p_ts_str = NULL;
	}

	return Status;
}