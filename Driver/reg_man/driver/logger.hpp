#pragma once
#include <ntifs.h>
#include <stdint.h>
#include <wchar.h>

#define DBG_PRINT(s, ...)	DbgPrintEx(77, 0, "[gawaln] %s\n", s, ##__VA_ARGS__)

namespace Logger
{
	UNICODE_STRING registryPath[];
	inline LARGE_INTEGER cookie{};

	NTSTATUS InstallRoutine(PDRIVER_OBJECT drvObject);
	NTSTATUS RegistryCallback(PVOID callbackContext, PVOID arg1, PVOID arg2);
	BOOLEAN CheckRegistry(PVOID rootObject, PUNICODE_STRING completeName);
	BOOLEAN CheckPath(PUNICODE_STRING ketyPath);

	NTSTATUS DeleteRoutine();
}