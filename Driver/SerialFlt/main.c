#include <ntddk.h>
#include <ntstrsafe.h>

/**********************************************************
�ļ�����:comdrv.c
�ļ�·��:./comdrv/comdrv.c
����ʱ��:2013-1-29,14:18:08
�ļ�����:Ů������
�ļ�˵��:�ó����Ƕ�<<��������>>��comcap���ڹ�����������ĸĽ���,
�Ľ�:
    [+] �����˶Դ��ڵĶ�ȡ�����Ĺ���(ԭ��ֻ��д����)
    [-] ȥ����ԭ�������д���(32��)�Ĺ���,����ֻ��ָ���˿�
    [*] �Ż��˲��ֳ���
**********************************************************/

//�������������
ULONG gReadRequestionCounter = 0;

//�ҵ��豸 �豸��չ����
typedef struct{
    PDEVICE_OBJECT pFltObj;
    PDEVICE_OBJECT pTopDev;
}COMDRV_DEVEXT,*PCOMDRV_DEVEXT;

//������ǰ������
NTSTATUS comdrvOpenCom(ULONG comid, PDEVICE_OBJECT* ppOldObj);
NTSTATUS comdrvAttachDevice(PDRIVER_OBJECT pDriver, PDEVICE_OBJECT pOldObj);

void comdrvDriverUnload(PDRIVER_OBJECT pDriverObject);
NTSTATUS DriverEntry(IN PDRIVER_OBJECT  pDriverObject, IN PUNICODE_STRING  pRegistryPath);

void PrintHexString(char* prefix, UCHAR* buffer, ULONG length);

NTSTATUS comdrvDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp);
NTSTATUS comdrvWrite(PDEVICE_OBJECT pDevice, PIRP pIrp);
NTSTATUS comdrvRead(PDEVICE_OBJECT pDevice, PIRP pIrp);
NTSTATUS comdrvReadCompletion(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);


//��ָ��ID�ŵĴ����豸����
NTSTATUS comdrvOpenCom(ULONG comid, PDEVICE_OBJECT* ppOldObj)
{
    NTSTATUS status;
    UNICODE_STRING name;
    WCHAR str[32] = {0};
    PFILE_OBJECT fileobj = NULL;
    PDEVICE_OBJECT devobj = NULL;
   // RtlStringCchPrintfW(str, 32, L"\\Device\\Serial%d", comid);
    RtlInitUnicodeString(&name, L"\\Device\\0000002c");
    status = IoGetDeviceObjectPointer(&name, FILE_ALL_ACCESS, &fileobj, &devobj);
    if(status == STATUS_SUCCESS)
        ObDereferenceObject(fileobj);
    *ppOldObj = devobj;
    return status;
}

//��ָ���Ĺ����豸��Ŀ���豸
NTSTATUS comdrvAttachDevice(PDRIVER_OBJECT pDriver, PDEVICE_OBJECT pOldObj)
{
    NTSTATUS status;
    PDEVICE_OBJECT pTopDev = NULL;
    PDEVICE_OBJECT pFltObj;
    PDEVICE_OBJECT next;
    PCOMDRV_DEVEXT pDevExt;
    //���������豸
    status = IoCreateDevice(pDriver, sizeof(COMDRV_DEVEXT), NULL, pOldObj->DeviceType, 0, TRUE, &pFltObj);
    if(status != STATUS_SUCCESS)
        return status;
    //������Ҫ��־λ
    if(pOldObj->Flags & DO_BUFFERED_IO) pFltObj->Flags |= DO_BUFFERED_IO;
    if(pOldObj->Flags & DO_DIRECT_IO) pFltObj->Flags |= DO_DIRECT_IO;
    if(pOldObj->Characteristics & FILE_DEVICE_SECURE_OPEN)
        pFltObj->Flags |= DO_POWER_PAGABLE;
    //�󶨵���һ�豸��
    pTopDev = IoAttachDeviceToDeviceStack(pFltObj, pOldObj);
    if(pTopDev == NULL){
        IoDeleteDevice(pFltObj);
        pFltObj = NULL;
        status = STATUS_UNSUCCESSFUL;
        return status;
    }
    pFltObj->Flags &= ~DO_DEVICE_INITIALIZING;
    pDevExt = (PCOMDRV_DEVEXT)pFltObj->DeviceExtension;
    pDevExt->pFltObj = pFltObj;
    pDevExt->pTopDev = pTopDev;
    return STATUS_SUCCESS;
}

//��ӡ���ڴ��16����
void PrintHexString(char* prefix, UCHAR* buffer, ULONG length)
{
    //int x;
    //char* outbuf = NULL;
    //char* poutbuf = NULL;
    ////char* prefix = "comdrv_write:";
    //int buflen = length*2+2+strlen(prefix);
    //do{
    //    if(length == 0) break;
    //    outbuf = (char*)ExAllocatePool(NonPagedPool, buflen);
    //    if(outbuf == NULL){
    //        KdPrint(("ExAllocatePool failed!\r\n"));
    //        break;
    //    }
    //    strcpy(outbuf, prefix);
    //    poutbuf = outbuf+strlen(prefix);
    //    for(x=0; x<length; x++){
    //        sprintf(poutbuf, "%02X", buffer[x]);
    //        poutbuf += 2;
    //    }
    //    sprintf(poutbuf, "\r\n");
    //    KdPrint((outbuf));
    //    ExFreePool(outbuf);
    //}while(0);

    int x;
    char* outbuf = NULL;
    char* poutbuf = NULL;
    //char* prefix = "comdrv_write:";
    int buflen = length*2+2+strlen(prefix);
    do{
        if(length == 0) break;
        outbuf = (char*)ExAllocatePool(NonPagedPool, buflen);
        if(outbuf == NULL){
            KdPrint(("ExAllocatePool failed!\r\n"));
            break;
        }
        strcpy(outbuf, prefix);
        poutbuf = outbuf+strlen(prefix);
        KdPrint(("%s", prefix));
        for(x=0; x<length; x++){
            sprintf(poutbuf, "%02X", buffer[x]);
            KdPrint(("%c", buffer[x]));
            poutbuf += 2;
        }
        KdPrint(("\r\n"));
        sprintf(poutbuf, "\r\n");
        KdPrint((outbuf));
        ExFreePool(outbuf);
    }while(0);
    
}

//��д�����Ĵ���
NTSTATUS comdrvWrite(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    NTSTATUS status;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    PCOMDRV_DEVEXT pDevExt = (PCOMDRV_DEVEXT)pDevice->DeviceExtension;
    ULONG len = pIrpSp->Parameters.Write.Length;
    PUCHAR buf = NULL;
    if(pIrp->MdlAddress != NULL)
        buf = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
    else
        buf = (PUCHAR)pIrp->UserBuffer;
    if(buf == NULL)
        buf = (PUCHAR)pIrp->AssociatedIrp.SystemBuffer;

    //��ӡ������
    if (buf != NULL)
    {
        PrintHexString("comdrv_write:", buf, len);
        //UCHAR tmp_str[2048] = { 0 };
        //RtlStringCchCopyA(tmp_str, len, buf);
        //DbgPrint("comdrv_write: [len] {%d} {%s} \n",len, tmp_str);
    }
    IoSkipCurrentIrpStackLocation(pIrp);
    return IoCallDriver(pDevExt->pTopDev, pIrp);
}

//�Զ���ɵĴ���
NTSTATUS comdrvReadCompletion(PDEVICE_OBJECT  pDeviceObject, PIRP  pIrp, PVOID  pContext)
{
    PIO_STACK_LOCATION CurIrp = NULL;
    ULONG length = 0;
    PUCHAR buffer = NULL;
    ULONG i;
    CurIrp = IoGetCurrentIrpStackLocation(pIrp);
    //�ڲ����ɹ���ǰ���½��в���
    if(NT_SUCCESS(pIrp->IoStatus.Status)){
        //�õ����������
        if(pIrp->MdlAddress != NULL)
            buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
        else
            buffer = (PUCHAR)pIrp->UserBuffer;
        if (buffer == NULL)
            buffer = (PUCHAR)pIrp->AssociatedIrp.SystemBuffer;

        // buffer = (PUCHAR)pIrp->AssociatedIrp.SystemBuffer;
        length = pIrp->IoStatus.Information;
        //��ӡ����ȡ��������
        if (buffer != NULL)
        {
            PrintHexString("comdrv_read:", buffer, length);
        }

    }
    //�����������,����ƽ��
    gReadRequestionCounter--;

    if(pIrp->PendingReturned)
        IoMarkIrpPending(pIrp);
    return pIrp->IoStatus.Status;
}

//�Զ������Ĵ���
NTSTATUS comdrvRead(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    NTSTATUS status;
    PCOMDRV_DEVEXT pDevExt = NULL;
    PIO_STACK_LOCATION CurIrp = NULL;
    //������
    if(pIrp->CurrentLocation == 1){
        ULONG ReturnedInfo = 0;
        KdPrint(("comdrv irp current location error\r\n"));
        status = STATUS_INVALID_DEVICE_REQUEST;
        pIrp->IoStatus.Status = status;
        pIrp->IoStatus.Information = ReturnedInfo;
        return status;
    }
    //����������
    gReadRequestionCounter++;

    pDevExt = (PCOMDRV_DEVEXT)pDevice->DeviceExtension;
    //���ö���ɻص��������·�IRP
    CurIrp = IoGetCurrentIrpStackLocation(pIrp);
    IoCopyCurrentIrpStackLocationToNext(pIrp);
    IoSetCompletionRoutine(pIrp, comdrvReadCompletion, NULL, TRUE, TRUE, TRUE);
    return IoCallDriver(pDevExt->pTopDev, pIrp);
}

//ͨ�÷ַ�����
NTSTATUS comdrvDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    NTSTATUS status;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    PCOMDRV_DEVEXT pDevExt = (PCOMDRV_DEVEXT)pDevice->DeviceExtension;

    //�������е�Դ����
    if(pIrpSp->MajorFunction == IRP_MJ_POWER){
        PoStartNextPowerIrp(pIrp);
        IoSkipCurrentIrpStackLocation(pIrp);
        return PoCallDriver(pDevExt->pTopDev, pIrp);
    }
    //ֱ���·���������
    IoSkipCurrentIrpStackLocation(pIrp);
    return IoCallDriver(pDevExt->pTopDev, pIrp);
}

void comdrvDriverUnload(PDRIVER_OBJECT pDriverObject)
{
#define  DELAY_ONE_MICROSECOND  (-10)
#define  DELAY_ONE_MILLISECOND (DELAY_ONE_MICROSECOND*1000)
#define  DELAY_ONE_SECOND (DELAY_ONE_MILLISECOND*1000)
    LARGE_INTEGER li;
    PCOMDRV_DEVEXT pDevExt = NULL;
    li.QuadPart = 3*1000*DELAY_ONE_MILLISECOND;
    pDevExt = (PCOMDRV_DEVEXT)pDriverObject->DeviceObject->DeviceExtension;
    IoDetachDevice(pDevExt->pTopDev);
    while(gReadRequestionCounter)
        KeDelayExecutionThread(KernelMode, FALSE, &li);
    IoDeleteDevice(pDevExt->pFltObj);
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT  pDriverObject, IN PUNICODE_STRING  pRegistryPath)
{
    NTSTATUS status;
    int it;
    PDEVICE_OBJECT pDevObj = NULL;
    //����ֻ���˶�д����,�����Ĳ��������²���������
    for(it=0; it<IRP_MJ_MAXIMUM_FUNCTION; it++)
        pDriverObject->MajorFunction[it] = comdrvDispatch;
    pDriverObject->MajorFunction[IRP_MJ_WRITE] = comdrvWrite;
    pDriverObject->MajorFunction[IRP_MJ_READ] = comdrvRead;
    pDriverObject->DriverUnload = comdrvDriverUnload;

    status = comdrvOpenCom(1, &pDevObj);
    if(!NT_SUCCESS(status))
        return status;
    status = comdrvAttachDevice(pDriverObject, pDevObj);
    if(!NT_SUCCESS(status))
        return status;
    return STATUS_SUCCESS;
}