#include <ntddk.h>
#include <ntstrsafe.h>

/**********************************************************
文件名称:comdrv.c
文件路径:./comdrv/comdrv.c
创建时间:2013-1-29,14:18:08
文件作者:女孩不哭
文件说明:该程序是对<<寒江独钓>>中comcap串口过滤驱动程序的改进版,
改进:
    [+] 增加了对串口的读取操作的过滤(原来只有写操作)
    [-] 去掉了原来对所有串口(32个)的过滤,现在只对指定端口
    [*] 优化了部分程序
**********************************************************/

//读操作请求计数
ULONG gReadRequestionCounter = 0;

//我的设备 设备扩展定义
typedef struct{
    PDEVICE_OBJECT pFltObj;
    PDEVICE_OBJECT pTopDev;
}COMDRV_DEVEXT,*PCOMDRV_DEVEXT;

//函数的前向声明
NTSTATUS comdrvOpenCom(ULONG comid, PDEVICE_OBJECT* ppOldObj);
NTSTATUS comdrvAttachDevice(PDRIVER_OBJECT pDriver, PDEVICE_OBJECT pOldObj);

void comdrvDriverUnload(PDRIVER_OBJECT pDriverObject);
NTSTATUS DriverEntry(IN PDRIVER_OBJECT  pDriverObject, IN PUNICODE_STRING  pRegistryPath);

void PrintHexString(char* prefix, UCHAR* buffer, ULONG length);

NTSTATUS comdrvDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp);
NTSTATUS comdrvWrite(PDEVICE_OBJECT pDevice, PIRP pIrp);
NTSTATUS comdrvRead(PDEVICE_OBJECT pDevice, PIRP pIrp);
NTSTATUS comdrvReadCompletion(PDEVICE_OBJECT  DeviceObject, PIRP  Irp, PVOID  Context);


//打开指定ID号的串口设备对象
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

//用指定的过虑设备绑定目标设备
NTSTATUS comdrvAttachDevice(PDRIVER_OBJECT pDriver, PDEVICE_OBJECT pOldObj)
{
    NTSTATUS status;
    PDEVICE_OBJECT pTopDev = NULL;
    PDEVICE_OBJECT pFltObj;
    PDEVICE_OBJECT next;
    PCOMDRV_DEVEXT pDevExt;
    //创建过虑设备
    status = IoCreateDevice(pDriver, sizeof(COMDRV_DEVEXT), NULL, pOldObj->DeviceType, 0, TRUE, &pFltObj);
    if(status != STATUS_SUCCESS)
        return status;
    //拷贝重要标志位
    if(pOldObj->Flags & DO_BUFFERED_IO) pFltObj->Flags |= DO_BUFFERED_IO;
    if(pOldObj->Flags & DO_DIRECT_IO) pFltObj->Flags |= DO_DIRECT_IO;
    if(pOldObj->Characteristics & FILE_DEVICE_SECURE_OPEN)
        pFltObj->Flags |= DO_POWER_PAGABLE;
    //绑定到另一设备上
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

//打印出内存的16进制
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

//对写操作的处理
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

    //打印出内容
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

//对读完成的处理
NTSTATUS comdrvReadCompletion(PDEVICE_OBJECT  pDeviceObject, PIRP  pIrp, PVOID  pContext)
{
    PIO_STACK_LOCATION CurIrp = NULL;
    ULONG length = 0;
    PUCHAR buffer = NULL;
    ULONG i;
    CurIrp = IoGetCurrentIrpStackLocation(pIrp);
    //在操作成功的前提下进行操作
    if(NT_SUCCESS(pIrp->IoStatus.Status)){
        //得到输出缓冲区
        if(pIrp->MdlAddress != NULL)
            buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);
        else
            buffer = (PUCHAR)pIrp->UserBuffer;
        if (buffer == NULL)
            buffer = (PUCHAR)pIrp->AssociatedIrp.SystemBuffer;

        // buffer = (PUCHAR)pIrp->AssociatedIrp.SystemBuffer;
        length = pIrp->IoStatus.Information;
        //打印出读取到的内容
        if (buffer != NULL)
        {
            PrintHexString("comdrv_read:", buffer, length);
        }

    }
    //读操作已完成,保持平衡
    gReadRequestionCounter--;

    if(pIrp->PendingReturned)
        IoMarkIrpPending(pIrp);
    return pIrp->IoStatus.Status;
}

//对读操作的处理
NTSTATUS comdrvRead(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    NTSTATUS status;
    PCOMDRV_DEVEXT pDevExt = NULL;
    PIO_STACK_LOCATION CurIrp = NULL;
    //错误处理
    if(pIrp->CurrentLocation == 1){
        ULONG ReturnedInfo = 0;
        KdPrint(("comdrv irp current location error\r\n"));
        status = STATUS_INVALID_DEVICE_REQUEST;
        pIrp->IoStatus.Status = status;
        pIrp->IoStatus.Information = ReturnedInfo;
        return status;
    }
    //读操作增加
    gReadRequestionCounter++;

    pDevExt = (PCOMDRV_DEVEXT)pDevice->DeviceExtension;
    //设置读完成回调函数并下发IRP
    CurIrp = IoGetCurrentIrpStackLocation(pIrp);
    IoCopyCurrentIrpStackLocationToNext(pIrp);
    IoSetCompletionRoutine(pIrp, comdrvReadCompletion, NULL, TRUE, TRUE, TRUE);
    return IoCallDriver(pDevExt->pTopDev, pIrp);
}

//通用分发函数
NTSTATUS comdrvDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
    NTSTATUS status;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
    PCOMDRV_DEVEXT pDevExt = (PCOMDRV_DEVEXT)pDevice->DeviceExtension;

    //忽略所有电源操作
    if(pIrpSp->MajorFunction == IRP_MJ_POWER){
        PoStartNextPowerIrp(pIrp);
        IoSkipCurrentIrpStackLocation(pIrp);
        return PoCallDriver(pDevExt->pTopDev, pIrp);
    }
    //直接下发所有请求
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
    //我们只过滤读写操作,其它的操作交给下层驱动处理
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