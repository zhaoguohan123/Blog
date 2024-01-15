#include <ntddk.h>
#include <windef.h>
#include <wdm.h>

#include "Common.h"
#include "AutoLock.h"

Globals g_Globals;

DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH DriverCreateClose, DriverDeviceControl;
void PushItem(LIST_ENTRY* entry);
NTSTATUS OnRegistryNotify(PVOID context, PVOID arg1, PVOID arg2);

extern "C"
{
	NTSTATUS DriverEntry(PDRIVER_OBJECT drvObject, PUNICODE_STRING regPath)
	{
		DbgPrint("[Troila] entry point called");

		NTSTATUS status = STATUS_SUCCESS;

		UNICODE_STRING device_name = RTL_CONSTANT_STRING(L"\\Device\\" DEVICE_NAME);
		UNICODE_STRING symbol_name = RTL_CONSTANT_STRING(L"\\??\\" DEVICE_NAME);

		PDEVICE_OBJECT device_obj = nullptr;

		bool symlinkCreated = false;

		InitializeListHead(&g_Globals.ItemsHead);

		g_Globals.ItemCount = 0;

		g_Globals.Mutex.Init();
		do
		{
			// Create the device
			status = IoCreateDevice(drvObject, 0, &device_name, FILE_DEVICE_UNKNOWN, 0, TRUE, &device_obj);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("[Troila]failed to create device(0x % 08X)", status);
				break;
			}
			// Create the symbol
			status = IoCreateSymbolicLink(&symbol_name, &device_name);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("[Troila]failed to create sym link (0x%08X)", status);
				break;
			}
			symlinkCreated = true;

			UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"7657.124");

			status = CmRegisterCallbackEx(OnRegistryNotify, &altitude, drvObject, nullptr, &g_Globals.RegCookie, nullptr);
			if (!NT_SUCCESS(status)) {
				DbgPrint("[Troila]failed to set registry callback(status = % 08X)", status);
				break;
			}

		} while (false);


		if (!NT_SUCCESS(status))
		{
			if (symlinkCreated)
			{
				IoDeleteSymbolicLink(&symbol_name);
			}

			if (device_obj)
			{
				IoDeleteDevice(device_obj);
			}

		}

		drvObject->DriverUnload = DriverUnload;
		drvObject->MajorFunction[IRP_MJ_CREATE] = drvObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
		drvObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;

		return status;
	}

}

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	auto status = CmUnRegisterCallback(g_Globals.RegCookie);
	if (!NT_SUCCESS(status)) 
	{
		KdPrint(("[Troila]failed on CmUnRegisterCallback (0x%08X)\n", status));
	}

	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\" DEVICE_NAME);

	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);

	while (!IsListEmpty(&g_Globals.ItemsHead))
	{
		auto entry = RemoveHeadList(&g_Globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<RegKeyProtectInfo>, Entry));
	}
	g_Globals.ItemCount = 0;

	return;
}

NTSTATUS DriverCreateClose(_In_ PDEVICE_OBJECT, _In_ PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


void PushItem(LIST_ENTRY* entry)
{
	AutoLock<FastMutex> lock(g_Globals.Mutex); // till now to the end of the function we will have Mutex
											   // and will be freed on destructor at the end of the function
	if (g_Globals.ItemCount > MaxRegKeyCount)
	{
		// too many items, remove oldest one
		auto head = RemoveHeadList(&g_Globals.ItemsHead);
		g_Globals.ItemCount--;
		ExFreePool(CONTAINING_RECORD(head, FullItem<RegKeyProtectInfo*>, Entry));
	}

	InsertTailList(&g_Globals.ItemsHead, entry);
	g_Globals.ItemCount++;
}


// 和R3层通信
NTSTATUS DriverDeviceControl(_In_ PDEVICE_OBJECT, _In_ PIRP Irp)
{
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	DWORD len = 0;

	switch (IrpStack->Parameters.DeviceIoControl.IoControlCode)
	{
		case IOCTL_REGKEY_PROTECT_ADD:
		{
			auto inputBufferSize = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
			auto inputBuffer = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;

			if (inputBufferSize == 0 || inputBuffer == nullptr)
			{
				KdPrint(("[Troila]The Registry Key passed is not Correct."));
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			KdPrint(("[Troila] The Registry Path to Protect is: %ws", inputBuffer));

			auto size = sizeof(FullItem<RegKeyProtectInfo>);
			auto info = (FullItem<RegKeyProtectInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
			if (info == nullptr)
			{
				KdPrint(("[Troila]Failed to Allocate Memory."));
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			RtlZeroMemory(info, size);

			auto& item = info->Data;
			//auto RegKeyLength = inputBufferSize / sizeof(WCHAR);
			RtlCopyMemory(item.KeyName, inputBuffer, inputBufferSize);
			PushItem(&info->Entry);
			break;
		}


		case IOCTL_REGKEY_PROTECT_REMOVE:
		{
			auto inputBufferSize = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
			auto inputBuffer = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;

			if (inputBufferSize == 0 || inputBuffer == nullptr)
			{
				KdPrint(("[Troila]The Registry Key passed is not Correct.\n"));
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			KdPrint(("[Troila] The Registry Path to Protect is: %ws", inputBuffer));

			AutoLock<FastMutex> lock(g_Globals.Mutex);

			UNICODE_STRING inputBF;
			RtlInitUnicodeString(&inputBF, inputBuffer);

			for (auto i = 0; i < g_Globals.ItemCount; i++)
			{
				auto entry = RemoveHeadList(&g_Globals.ItemsHead);
				auto info = CONTAINING_RECORD(entry, FullItem<RegKeyProtectInfo*>, Entry);
				auto kName = (WCHAR*)&info->Data;

				UNICODE_STRING tbcName;
				RtlInitUnicodeString(&tbcName, kName);

				if (RtlCompareUnicodeString(&inputBF, &tbcName, TRUE) == 0)
				{
					KdPrint(("[Troila] Found a Matching Protected key. Removing it from the LinkedList."));
					g_Globals.ItemCount--;
					(void)ExFreePool(CONTAINING_RECORD(entry, FullItem<RegKeyProtectInfo>, Entry));
					break;
				}
				InsertTailList(&g_Globals.ItemsHead, entry);
			}
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		case IOCTL_REGKEY_PROTECT_CLEAR:
		{
			KdPrint(("[Troila] Sounding The Purge Siren! Removing all Protected RegKeys."));
			while (!IsListEmpty(&g_Globals.ItemsHead))
			{
				auto entry = RemoveHeadList(&g_Globals.ItemsHead);
				(void)ExFreePool(CONTAINING_RECORD(entry, FullItem<RegKeyProtectInfo>, Entry));
			}
			g_Globals.ItemCount = 0;

			break;
		}

		default:
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;

	}

	// complete the request
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}


NTSTATUS OnRegistryNotify(PVOID, PVOID arg1, PVOID arg2) {

	NTSTATUS status = STATUS_SUCCESS;

	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)arg1) 
	{
		case RegNtPreSetValueKey:
		{
			PREG_SET_VALUE_KEY_INFORMATION preInfo = static_cast<PREG_SET_VALUE_KEY_INFORMATION>(arg2);

			PCUNICODE_STRING keyName = nullptr;
			if (!NT_SUCCESS(CmCallbackGetKeyObjectID(&g_Globals.RegCookie, preInfo->Object, nullptr, &keyName))) 
			{
				KdPrint(("[Troila]CmCallbackGetKeyObjectID failed", keyName));
				break;
			}
			KdPrint(("[Troila] KeyName(U_C) In Linked List is: %wZ\n", keyName));
			AutoLock<FastMutex> lock(g_Globals.Mutex);

			for (auto i = 0; i < g_Globals.ItemCount; i++)
			{
				PLIST_ENTRY entry = RemoveHeadList(&g_Globals.ItemsHead);

				auto info = CONTAINING_RECORD(entry, FullItem<RegKeyProtectInfo*>, Entry);
				auto kName = (WCHAR*)&info->Data;

				UNICODE_STRING tbcName;
				RtlInitUnicodeString(&tbcName, kName);
				KdPrint(("[Troila] tbcName(U_C) In Linked List is: %wZ!\n", tbcName));

				if (RtlCompareUnicodeString(keyName, &tbcName, FALSE) == 0)
				{
					KdPrint(("[Troila] Found a Matching Protected key. Blocking Any Modification Attempts."));
					InsertTailList(&g_Globals.ItemsHead, entry);
					status = STATUS_ACCESS_DENIED;
					break;
				}
				InsertTailList(&g_Globals.ItemsHead, entry);
			}
		}
	}

	return status;
}
