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
		// CmCallbackGetKeyObjectID 함수를 통해 root_object를 통해 root_object_name에 경로를 저장.
		if (!NT_SUCCESS(CmCallbackGetKeyObjectID(&Logger::cookie, rootObject, &rootObjectId, &rootObjectName)))
		{
			DBG_PRINT("Failed CmCallbackGetKeyObjectID");
			return FALSE;
		}

		// 만약 complete_name이 유효하다면 root_object_name과 complete_name의 문자열을 합친다.
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
		// complete_name은 절대 경로 또는 상대 경로를 가지는데 root_object가 유효하지 않으면 complete_name에 전체 경로가 있음을 의미.
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