﻿#include <iostream>
#include <Windows.h>

#define IOCTL_REGKEY_PROTECT_ADD	CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REGKEY_PROTECT_REMOVE	CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_REGKEY_PROTECT_CLEAR	CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

int Error(const char* text) {
	printf("%s (%d)\n", text, ::GetLastError());
	return 1;
}

int PrintUsage() {
	printf("Usage: RegistryProtectorClient <option> [directory]\n");
	printf("\tOption: add, remove or clear\n");
	return 0;
}

int wmain(int argc, const wchar_t* argv[]) {
	if (argc < 2) {
		return PrintUsage();
	}

	HANDLE hDevice = ::CreateFile(L"\\\\.\\RegProtectorDrv", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) 
	{
		return Error("Failed to open handle to device");
	}
		

	DWORD returned;
	BOOL success;
	bool badOption = false;

	if (::_wcsicmp(argv[1], L"add") == 0) {
		if (argc < 3)
			return PrintUsage();

		success = ::DeviceIoControl(hDevice, IOCTL_REGKEY_PROTECT_ADD, (PVOID)argv[2], ((DWORD)::wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returned, nullptr);
	}

	else if (::_wcsicmp(argv[1], L"remove") == 0) {
		if (argc < 3)
			return PrintUsage();

		success = ::DeviceIoControl(hDevice, IOCTL_REGKEY_PROTECT_REMOVE, (PVOID)argv[2], ((DWORD)::wcslen(argv[2]) + 1) * sizeof(WCHAR), nullptr, 0, &returned, nullptr);
	}

	else if (::_wcsicmp(argv[1], L"clear") == 0) {

		success = ::DeviceIoControl(hDevice, IOCTL_REGKEY_PROTECT_CLEAR, nullptr, 0, nullptr, 0, &returned, nullptr);
	}

	else {
		badOption = true;
		printf("Unknown option.\n");
	}


	::CloseHandle(hDevice);

	return 0;
}