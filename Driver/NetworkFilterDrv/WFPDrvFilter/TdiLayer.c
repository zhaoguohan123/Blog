#include "TdiLayer.h"
#include "FilterList.h"
#include "Comm.h"

PDEVICE_OBJECT filterDevObj = NULL;
PDEVICE_OBJECT doDevObj = NULL;

PDEVICE_OBJECT GetDoDeviceObj()
{
    return doDevObj;
}

NTSTATUS MyDispatch(PDEVICE_OBJECT deviceObj, PIRP irp)
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(irp);

    if (irpStack == NULL)
    {
        goto end;
    }
    if (irpStack->MinorFunction == TDI_CONNECT)
    {
        PTDI_REQUEST_KERNEL_CONNECT tdiConn = (PTDI_REQUEST_KERNEL_CONNECT)(&irpStack->Parameters);
        PTRANSPORT_ADDRESS sportAddr = (PTRANSPORT_ADDRESS)(tdiConn->RequestConnectionInformation->RemoteAddress);
        PTA_ADDRESS taAddr = sportAddr->Address;
        PTDI_ADDRESS_IP tdiAddr = taAddr->Address;
        DWORD address = tdiAddr->in_addr;
        // 大端转小端
        address = ((address & 0xFF) << 24) + (((address >> 8) & 0xFF) << 16) + (((address >> 16) & 0xFF) << 8) + (((address >> 24) & 0xFF));
        if (address == 0x7F000001) // 127.0.0.1
        {
            goto end;
        }

        FILTER filter = { 0 };
        filter.filterIndex = SCREEN_SHOT;
        filter.ip = address;
        
        if (RtlQueryFilterToList(&filter, FALSE))
        {
            // 通知三环截图,每次访问每次通知
            NotifyHitWeb();
            
            // 等待通知结果
            if (NT_SUCCESS(WaitNotifyHitWeb(2000)))
            {
                if (IsBlockWeb())	// 阻塞网络
                {
                    KdPrint(("[info]:TdiLayer_MyDispatch -- 阻塞这次网络\r\n"));
                    irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
                    irp->IoStatus.Information = 0;
                    IoCompleteRequest(irp, IO_NO_INCREMENT);
                    return STATUS_INVALID_DEVICE_REQUEST;
                }
            }
            else
            {
                KdPrint(("[error]:TdiLayer_MyDispatch -- 超时\r\n"));
            }
            KdPrint((" %u.%u.%u.%u \r\n", (address >> 24) & 0xFF, (address >> 16) & 0xFF, (address >> 8) & 0xFF, (address) & 0xFF));
        }
    }
end:
    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(doDevObj, irp);
}

VOID UnInitTdiLayer(PDRIVER_OBJECT pDrv)
{
    if (doDevObj)
    {
        IoDetachDevice(doDevObj);
    }
    //if (filterDevObj)
    //{
    //    // 去掉不蓝
    //    //IoDeleteDevice(filterDevObj);
    //}
    KdPrint(("[info]:InitTdiLayer_UnInitTdiLayer  \r\n"));
}

NTSTATUS InitTdiLayer(PDRIVER_OBJECT pDrv)
{
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING targetDeviceName;
    status = IoCreateDevice(pDrv, 0, NULL, FILE_DEVICE_NETWORK, FILE_DEVICE_SECURE_OPEN, FALSE, &filterDevObj);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("[error]:InitTdiLayer_IoCreateDevice error \r\n"));
        return status;
    }
    pDrv->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = MyDispatch;
    pDrv->Flags &= ~DO_DEVICE_INITIALIZING;

    RtlInitUnicodeString(&targetDeviceName, L"\\Device\\Tcp");

    status = IoAttachDevice(filterDevObj, &targetDeviceName, &doDevObj);
    if (!NT_SUCCESS(status))
    {
        KdPrint(("[error]:InitTdiLayer_IoAttachDevice error \r\n"));
        IoDeleteDevice(filterDevObj);
        return status;
    }
}