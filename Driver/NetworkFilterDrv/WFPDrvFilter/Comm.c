#include "Comm.h"
#include "FilterList.h"
#include "ProcessTime.h"
#include "ProcessProhibition.h"
#include "TdiLayer.h"

#define IOCTL_SET_MAILWEB_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN,0x900,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_SET_RECORDTIME_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN,0x901,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_SET_PROHIBITION_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN,0x902,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_FILTER_SET CTL_CODE(FILE_DEVICE_UNKNOWN,0x903,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_FILTER_DROP CTL_CODE(FILE_DEVICE_UNKNOWN,0x904,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_BLOCK_MAILWEB CTL_CODE(FILE_DEVICE_UNKNOWN,0x905,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_GET_RECORDTIME_INFO CTL_CODE(FILE_DEVICE_UNKNOWN,0x906,METHOD_OUT_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_GET_CURRECORDTIME_INFO CTL_CODE(FILE_DEVICE_UNKNOWN,0x907,METHOD_OUT_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_USER_GET_PROCESSINFO_CONTEX_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN,0x908,METHOD_OUT_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_USER_SET_CONTEX_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN,0x909,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

#define IOCTL_IP_FILTER_DROP_ALL		CTL_CODE(FILE_DEVICE_UNKNOWN,0x90a,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_BLOCK_ACCESS_DNS			CTL_CODE(FILE_DEVICE_UNKNOWN,0x90b,METHOD_IN_DIRECT,FILE_ANY_ACCESS)
#define IOCTL_ENABLE_IP_WHITE_LIST      CTL_CODE(FILE_DEVICE_UNKNOWN,0x90c,METHOD_IN_DIRECT,FILE_ANY_ACCESS)

BOOLEAN enableIPWhiteList = FALSE;
BOOLEAN blockAccessDns = FALSE;
BOOLEAN blockWebFlag = FALSE;
PKEVENT hitWebEvent = NULL;
PKEVENT recordTimeEvent = NULL;
PKEVENT prohibitEvent = NULL;
PDEVICE_OBJECT commDevice = NULL;
UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\commIolink");
UNICODE_STRING symbolName = RTL_CONSTANT_STRING(L"\\??\\commIolink");
//UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\kl_gse_device");
//UNICODE_STRING symbolName = RTL_CONSTANT_STRING(L"\\??\\kl_gse_iolink");

// �Ƿ���Ļ��ͼ
BOOLEAN IsBlockWeb()
{
	return blockWebFlag;
}

BOOLEAN IsEnalbeIPWhiteList()
{
    return enableIPWhiteList;
}

BOOLEAN IsBlockAccessDns()
{
    return blockAccessDns;
}

// ֪ͨӦ�ò���������������
LONG NotifyHitWeb()
{
	if (recordTimeEvent == NULL)
	{
		KdPrint(("[error]:Comm_NotifyHitWeb -- recordTimeEvent�¼�δ����\r\n"));
		return 0;
	}
	return KeSetEvent(hitWebEvent, IO_NO_INCREMENT, FALSE);
}

// �ȴ�֪ͨ��Ӧ - ����DPC Level��ʹ�ã�����timeOut����Ϊ0
NTSTATUS WaitNotifyHitWeb(ULONG waitMiliSeconds)
{
	if (hitWebEvent == NULL)
	{
		KdPrint(("[error]:Comm_WaitNotifyHitWeb -- hitWebEvent�¼�δ����\r\n"));
		return STATUS_UNSUCCESSFUL;
	}
	if (waitMiliSeconds != 0)
	{
		LARGE_INTEGER timeOut = { 0 };
		timeOut.QuadPart = -10 * 1000;
		timeOut.QuadPart *= waitMiliSeconds;
		return KeWaitForSingleObject(hitWebEvent, Executive, KernelMode, FALSE, &timeOut);
	}
	return KeWaitForSingleObject(hitWebEvent, Executive, KernelMode, FALSE, NULL);
}

// ֪ͨӦ�ò��ȡ��¼ʱ��
LONG NotifyRecordTime()
{
	if (recordTimeEvent == NULL)
	{
		KdPrint(("[error]:Comm_NotifyRecordTime -- recordTimeEvent�¼�δ����\r\n"));
		return 0;
	}
	return KeSetEvent(recordTimeEvent, IO_NO_INCREMENT, FALSE);
}

// �ȴ���ȡ��¼ʱ���֪ͨ��Ӧ
NTSTATUS WaitNotifyRecordTime(ULONG waitMiliSeconds)
{
	if (recordTimeEvent == NULL)
	{
		KdPrint(("[error]:Comm_WaitNotifyRecordTime -- recordTimeEvent�¼�δ����\r\n"));
		return STATUS_UNSUCCESSFUL;
	}
	if (waitMiliSeconds != 0)
	{
		LARGE_INTEGER timeOut = { 0 };
		timeOut.QuadPart = -10 * 1000;
		timeOut.QuadPart *= waitMiliSeconds;
		return KeWaitForSingleObject(recordTimeEvent, Executive, KernelMode, FALSE, &timeOut);
	}
	return KeWaitForSingleObject(recordTimeEvent, Executive, KernelMode, FALSE, NULL);
}

// ֪ͨ��ȡ������Ϣ�����ý��̽���
LONG NotifyProcessProhibition()
{
	if (prohibitEvent == NULL)
	{
		KdPrint(("[error]:Comm_NotifyProcessProhibition -- prohibitEvent�¼�δ����\r\n"));
		return 0;
	}
	return KeSetEvent(prohibitEvent, IO_NO_INCREMENT, FALSE);
}

// �ȴ����̽����������
NTSTATUS WaitNotifyProcessProhibition(ULONG waitMiliSeconds)
{
	if (prohibitEvent == NULL)
	{
		KdPrint(("[error]:Comm_WaitNotifyProcessProhibition -- prohibitEvent�¼�δ����\r\n"));
		return STATUS_UNSUCCESSFUL;
	}
	if (waitMiliSeconds != 0)
	{
		LARGE_INTEGER timeOut = { 0 };
		timeOut.QuadPart = -10 * 1000;
		timeOut.QuadPart *= waitMiliSeconds;
		return KeWaitForSingleObject(prohibitEvent, Executive, KernelMode, FALSE, &timeOut);
	}
	return KeWaitForSingleObject(prohibitEvent, Executive, KernelMode, FALSE, NULL);
}


// IoControlͨ��
NTSTATUS IoCommFunc(DEVICE_OBJECT* deviceObject, IRP* irp)
{
	NTSTATUS stat = STATUS_SUCCESS;
	PIO_STACK_LOCATION ioLocation = IoGetCurrentIrpStackLocation(irp);
	PVOID ioBuffer = irp->AssociatedIrp.SystemBuffer;
	PVOID outBuffer = irp->AssociatedIrp.SystemBuffer;
	ULONG code = ioLocation->Parameters.DeviceIoControl.IoControlCode;

	ULONG inLen = ioLocation->Parameters.DeviceIoControl.InputBufferLength;		// ����ϵͳ����Ӧ�ò��������ݳ������
	ULONG outLen = ioLocation->Parameters.DeviceIoControl.OutputBufferLength;	// ����ϵͳ����Ӧ�ò����󳤶����

	switch (code)
	{
	case IOCTL_SET_MAILWEB_EVENT:
	{
		if (inLen == sizeof(PKEVENT))
		{
			PKEVENT tmpEvent = *(PKEVENT*)ioBuffer;
			if (NT_SUCCESS(ObReferenceObjectByHandle(tmpEvent, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, &tmpEvent, NULL)))
			{
				if (hitWebEvent) ObDereferenceObject(hitWebEvent);
				hitWebEvent = tmpEvent;
			}
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_SET_MAILWEB_EVENT ����������ҳ�¼�\r\n"));
		break;
	}
	case IOCTL_SET_RECORDTIME_EVENT:
	{
		if (inLen == sizeof(PKEVENT))
		{
			PKEVENT tmpEvent = *(PKEVENT*)ioBuffer;
			if (NT_SUCCESS(ObReferenceObjectByHandle(tmpEvent, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, &tmpEvent, NULL)))
			{
				if (recordTimeEvent) ObDereferenceObject(recordTimeEvent);
				recordTimeEvent = tmpEvent;
			}
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_SET_RECORDTIME_EVENT ���ü�¼����ʱ���¼�\r\n"));
		break;
	}
	case IOCTL_SET_PROHIBITION_EVENT:
	{
		if (inLen == sizeof(PKEVENT))
		{
			PKEVENT tmpEvent = *(PKEVENT*)ioBuffer;
			if (NT_SUCCESS(ObReferenceObjectByHandle(tmpEvent, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, &tmpEvent, NULL)))
			{
				if (prohibitEvent) ObDereferenceObject(prohibitEvent);
				prohibitEvent = tmpEvent;
			}
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_SET_PROHIBITION_EVENT ���ý��̽����¼�\r\n"));
		break;
	}
	case IOCTL_FILTER_SET:
	{
		if (inLen == sizeof(FILTER))
		{
			RtlAddFilterToList((PFILTER)ioBuffer, TRUE);
		}
		DbgPrint(("<test>[info]:Comm_IoCommFunc -- IOCTL_FILTER_SET ��ӹ���\r\n"));
		break;
	}
	case IOCTL_FILTER_DROP:
	{
		if (inLen == sizeof(FILTER))
		{
			RtlDeleteFilterToList((PFILTER)ioBuffer);
		}
		DbgPrint(("<test>[info]:Comm_IoCommFunc -- IOCTL_FILTER_DROP ɾ������\r\n"));
		break;
	}

	case IOCTL_IP_FILTER_DROP_ALL:
	{
		RtlDeleteAllFilterToList();
		DbgPrint(("<test>[info]:Comm_IoCommFunc -- IOCTL_IP_FILTER_DROP_ALL ɾ�����й���\r\n"));
        break;
	}

    case IOCTL_BLOCK_ACCESS_DNS:
    {
        if (inLen == sizeof(BOOLEAN))
        {
            blockAccessDns = *(PBOOLEAN)ioBuffer;
        }

        DbgPrint("<test>[info]:Comm_IoCommFunc -- IOCTL_BLOCK_ACCESS_DNS �Ƿ���ֹ����DNS,blockAccessDns=%d\r\n", blockAccessDns);
        break;
    }

    case IOCTL_ENABLE_IP_WHITE_LIST:
    {
        if (inLen == sizeof(BOOLEAN))
        {
            enableIPWhiteList = *(PBOOLEAN)ioBuffer;
        }

        DbgPrint("<test>[info]:Comm_IoCommFunc -- IOCTL_BLOCK_ACCESS_DNS �Ƿ�����IP������,enableIPWhiteList=%d\r\n", enableIPWhiteList);
        break;
    }
	case IOCTL_BLOCK_MAILWEB:
	{
		if (inLen == sizeof(BOOLEAN))
		{
			blockWebFlag = *(PBOOLEAN)ioBuffer;
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_BLOCK_MAILWEB �Ƿ���ֹ�ʼ���ҳ\r\n"));
		break;
	}
	case IOCTL_GET_RECORDTIME_INFO:
	{
		ULONG size = sizeof(PROCESS_TIME_RECORD);
		if (outLen >= size)
		{
			outBuffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
			if (outBuffer != NULL)
			{
				PROCESS_TIME_RECORD record = { 0 };
				stat = GetTimeRecordBeNotify(&record, size);
				if (NT_SUCCESS(stat))
				{
					RtlCopyMemory(outBuffer, &record, size);
				}
			}
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_GET_RECORDTIME_INFO �������ʱ���¼[����:%d]\r\n", outLen));
		break;
	}
	case IOCTL_GET_CURRECORDTIME_INFO:
	{
		ULONG size = sizeof(PROCESS_TIME_RECORD);
		if (outLen >= size && inLen == sizeof(HANDLE))
		{
			outBuffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
			if (outBuffer != NULL)
			{
				HANDLE pid = *(PHANDLE)ioBuffer;
				PROCESS_TIME_RECORD record = { 0 };
				stat = GetSingleProcessRunTime(pid, &record);
				if (NT_SUCCESS(stat))
				{
					RtlCopyMemory(outBuffer, &record, size);
				}
			}
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_GET_CURRECORDTIME_INFO -- ���󵥸�����ʱ����Ϣ[���󳤶�:%d,���볤��:%d]\r\n", outLen, inLen));
		break;
	}
	case IOCTL_USER_GET_PROCESSINFO_CONTEX_CONTROL:
	{
		ULONG size = sizeof(PROCESS_CREATE_INFO);
		if (outLen >= size)
		{
			outBuffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
			if (outBuffer != NULL)
			{
				PROCESS_CREATE_INFO createInfo = { 0 };
				stat = GetCreateProcessInfoBeNotify(&createInfo, size);
				if (NT_SUCCESS(stat))
				{
					RtlCopyMemory(outBuffer, &createInfo, size);
				}
			}
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_USER_GET_PROCESSINFO_CONTEX_CONTROL -- [��������:%d]\r\n", outLen));
		break;
	}
	case IOCTL_USER_SET_CONTEX_CONTROL:
	{
		if (inLen == sizeof(PROCESS_RUNTIME_STAT))
		{
			SetProcessRuntimeStat((PPROCESS_RUNTIME_STAT)ioBuffer);
		}
		KdPrint(("[info]:Comm_IoCommFunc -- IOCTL_USER_SET_CONTEX_CONTROL -- [���볤��:%d]\r\n", inLen));
		break;
	}
	default:
		break;
	}

	irp->IoStatus.Information = 0;	// DO_DIRECT_IO����Ҫ������,����ϵͳ�Զ�����
	irp->IoStatus.Status = stat;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

// ��ͨ��
NTSTATUS  NullFunc(DEVICE_OBJECT* deviceObject, IRP* irp)
{
#if 0
	IoSkipCurrentIrpStackLocation(irp);
	return IoCallDriver(GetDoDeviceObj(), irp);
#else
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
#endif
}

// ----------------------------------------------------

VOID UnInitComm()
{
	IoDeleteSymbolicLink(&symbolName);
	if (commDevice != NULL)
	{
		IoDeleteDevice(commDevice);
	}
	return;
}

NTSTATUS InitializeComm(PDRIVER_OBJECT ObjDrv)
{
	NTSTATUS status = STATUS_SUCCESS;
	status = IoCreateDevice(ObjDrv, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &commDevice);
	if (NT_SUCCESS(status))
	{
		status = IoCreateSymbolicLink(&symbolName, &deviceName);
		if (NT_SUCCESS(status))
		{
			for (size_t i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
			{
				ObjDrv->MajorFunction[i] = NullFunc;
			}
			ObjDrv->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoCommFunc;
			commDevice->Flags |= DO_DIRECT_IO;
			commDevice->Flags &= ~DO_DEVICE_INITIALIZING;	// �����������λ����ô3�������޷����豸
			status = STATUS_SUCCESS;
		}
		else
		{
			IoDeleteDevice(commDevice);
			commDevice = NULL;
		}
	}
	return status;
} 