#include "Comm.h"
#include "WfpFilter.h"
#include "FilterList.h"
#include "ProcessTime.h"
#include "ProcessProhibition.h"
#include "TdiLayer.h"

VOID UnLoad(PDRIVER_OBJECT pDrv)
{
	UnInitProcessTimeRecorder(pDrv);

	UnInitProcessProhibition(pDrv);

	RtlUnInitFilterLists();

	UnInitComm();

	//UnInitTdiLayer(pDrv);

	UnInitWfp();

	KdPrint(("unload\r\n"));
}
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pRegPath)
{
    DbgPrint("<test>DriverEntry start");
	NTSTATUS status = STATUS_SUCCESS;
	RtlInitFilterLists();

	status = InitProcessTimeRecorder(pDriver);
	if (!NT_SUCCESS(status))
	{
		goto end;
	}

	status = InitializeComm(pDriver);
	if (!NT_SUCCESS(status))
	{
		goto end;
	}

	/*status = InitTdiLayer(pDriver);
	if (!NT_SUCCESS(status))
	{
		goto end;
	}*/

	status = InitializeWfp(pDriver);
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
	status = InitProcessProhibition(pDriver);
	if (!NT_SUCCESS(status))
	{
		goto end;
	}
end:
	pDriver->DriverUnload = UnLoad;
	return status;
}