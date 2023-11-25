#include <ntddk.h>
#include <wdm.h>
#include <dispmprt.h>

#pragma code_seg(push)
#pragma code_seg("INIT")
NTSTATUS DriverEntry(
          IN  PDRIVER_OBJECT DriverObject,
          IN  PUNICODE_STRING RegistryPath
        )
{
    DRIVER_INITIALIZATION_DATA DriverInitializationData = {'\0'};

    PAGED_CODE();

    if (! ARGUMENT_PRESENT(DriverObject) ||
        ! ARGUMENT_PRESENT(RegistryPath))
    {
        return STATUS_INVALID_PARAMETER;
    }
    DriverInitializationData.Version = DXGKDDI_INTERFACE_VERSION;

    return DxgkInitialize(DriverObject, RegistryPath, &DriverInitializationData);
}
