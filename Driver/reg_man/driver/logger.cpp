#include "logger.hpp"

UNICODE_STRING Logger::registryPath[] = {
	RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\ControlSet001\\Control"),
	RTL_CONSTANT_STRING(L"\\REGISTRY\\MACHINE\\System\\ControlSet001\\Enum")
};

NTSTATUS Logger::InstallRoutine(PDRIVER_OBJECT drvObject)
{
	UNICODE_STRING altitude{};
	RtlInitUnicodeString(&altitude, L"136800");

	if (!NT_SUCCESS(CmRegisterCallbackEx(
		Logger::RegistryCallback,
		&altitude,
		drvObject,
		nullptr,
		&Logger::cookie,
		nullptr
	)))
	{
		DBG_PRINT("Failed CmRegisterCallbackEx");
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		DBG_PRINT("Success CmRegisterCallbackEx");
		return STATUS_SUCCESS;
	}
}

NTSTATUS Logger::RegistryCallback(PVOID callbackContext, PVOID arg1, PVOID arg2)
{
	REG_NOTIFY_CLASS notifyClass = (REG_NOTIFY_CLASS)((ULONG_PTR)arg1);

	if (notifyClass == RegNtPreOpenKeyEx)
	{
		PREG_OPEN_KEY_INFORMATION_V1 openKeyInfo = (PREG_OPEN_KEY_INFORMATION_V1)arg2;
		if (CheckRegistry(openKeyInfo->RootObject, openKeyInfo->CompleteName))
		{
			DBG_PRINT("[ callback ] Success prevent!");
			return STATUS_ACCESS_DENIED;
		}
	}
}

BOOLEAN Logger::CheckRegistry(PVOID rootObject, PUNICODE_STRING completeName)
{
	PCUNICODE_STRING rootObjectName;
	ULONG_PTR rootObjectId;
	UNICODE_STRING keyPath{ 0 };

	if (rootObject)
	{
		// CmCallbackGetKeyObjectID �Լ��� ���� root_object�� ���� root_object_name�� ��θ� ����.
		if (!NT_SUCCESS(CmCallbackGetKeyObjectID(&Logger::cookie, rootObject, &rootObjectId, &rootObjectName)))
		{
			DBG_PRINT("Failed CmCallbackGetKeyObjectID");
			return FALSE;
		}

		// ���� complete_name�� ��ȿ�ϴٸ� root_object_name�� complete_name�� ���ڿ��� ��ģ��.
		if (completeName->Length && completeName->Buffer)
		{
			keyPath.MaximumLength = rootObjectName->Length + completeName->Length + (sizeof(WCHAR) * 2);
			keyPath.Buffer = (PWCH)ExAllocatePoolWithTag(NonPagedPool, keyPath.MaximumLength, 'awag');

			if (!keyPath.Buffer) {
				DBG_PRINT("Failed ExAllocatePoolWithTag");
				return FALSE;
			}

			swprintf(keyPath.Buffer, L"%wZ\\%wZ", rootObjectName, completeName);
			keyPath.Length = rootObjectName->Length + completeName->Length + (sizeof(WCHAR));

			return CheckPath(&keyPath);

		}
	}
	else
	{
		// complete_name�� ���� ��� �Ǵ� ��� ��θ� �����µ� root_object�� ��ȿ���� ������ complete_name�� ��ü ��ΰ� ������ �ǹ�.
		return CheckPath(completeName);

	}
	if (keyPath.Buffer) ExFreePoolWithTag(keyPath.Buffer, 'awag');
	return FALSE;
}

BOOLEAN Logger::CheckPath(PUNICODE_STRING ketyPath)
{
	BOOLEAN matched = FALSE;
	ULONG count = sizeof(Logger::registryPath) / sizeof(UNICODE_STRING);

	for (ULONG i = 0; i < count; ++i)
	{
		if (RtlEqualUnicodeString(ketyPath, &Logger::registryPath[i], TRUE))
		{
			matched = TRUE;
			break;
		}
	}

	return matched;
}

NTSTATUS Logger::DeleteRoutine()
{
	NTSTATUS status;

	status = CmUnRegisterCallback(Logger::cookie);
	if (!NT_SUCCESS(status))
	{
		DBG_PRINT("Failed CmUnRegisterCallback\n");
		return status;
	}
	else
	{
		DBG_PRINT("Success CmUnRegisterCallback\n");
		return status;
	}
}