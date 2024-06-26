#include <ntddk.h>
#include "wfp.h"


PDEVICE_OBJECT DeviceObject;

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	
	UnregisterNetwrokFilterUDP_OUT();

	UnregisterNetwrokFilterUDP_IN();

	IoDeleteDevice(DeviceObject);

	DbgPrint("salmon:[salmon] callout driver unloaded\n");
}


typedef struct FILTER_INFO
{
	FWPS_CALLOUT  def_callout;
	FWPM_CALLOUT  def_calloutm;
	FWPM_SUBLAYER def_sublayer;
	FWPM_FILTER   def_filter;
}FILTER_INFO, *PFILTER_INFO;


EXTERN_C NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status = STATUS_SUCCESS;

	status = IoCreateDevice(DriverObject, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("[salmon] failed to crate device object for callout driver\n");
		return status;
	}

	status = RegisterNetworkFilterUDP_OUT(DeviceObject);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = RegisterNetworkFilterUDP_IN(DeviceObject);
	if (!NT_SUCCESS(status)) 
	{
		return status;
	}

	DriverObject->DriverUnload = DriverUnload;
	DbgPrint("[salmon] callout filter driver has been registered and loaded\n");
	return status;
}