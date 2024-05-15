#include "util.h"
#include <ntimage.h>
#define SET_PVOID(Destination,Source)((*(PVOID*)(Destination))=((PVOID)(Source)))

// 过掉回调验证
ULONG RtlFakeCallBackVerify(PVOID ldr)
{
    //_disable();
    //__writecr0(__readcr0() & 0xfffffffffffeffff);   // 关写保护
    ULONG oriFlags = ((PKLDR_DATA_TABLE_ENTRY64)ldr)->Flags;
    ((PKLDR_DATA_TABLE_ENTRY64)ldr)->Flags |= 0x20;
    //_mm_clflush(&(((PKLDR_DATA_TABLE_ENTRY64)ldr)->Flags));
    //__writecr0(__readcr0() | 0x10000);
    //_enable();
    return oriFlags;
}

// 还原
VOID RtlResetCallBackVerify(PVOID ldr, ULONG oldFlags)
{
    //_disable();
    //__writecr0(__readcr0() & 0xfffffffffffeffff);
    ((PKLDR_DATA_TABLE_ENTRY64)ldr)->Flags = oldFlags;
    //__writecr0(__readcr0() | 0x10000);
    //_enable();
}

// 根据路径得到名称  PASSIVE_LEVEL
NTSTATUS RtlGetFileName(const PUNICODE_STRING pImageFilePath, PUNICODE_STRING pFileName)
{
	static UNICODE_STRING UnicodeString = RTL_CONSTANT_STRING(L"*\\*");
	if (MmIsAddressValid(pImageFilePath) == FALSE || pImageFilePath->Buffer == NULL || pImageFilePath->Length <= sizeof(wchar_t) || MmIsAddressValid(pFileName) == FALSE)
	{
		return STATUS_UNSUCCESSFUL;
	}
	if (pImageFilePath->Buffer[0] == OBJ_NAME_PATH_SEPARATOR || FsRtlIsNameInExpression(&UnicodeString, pImageFilePath, TRUE, NULL))
	{
		PWCHAR p;
		ULONG l;
		p = &pImageFilePath->Buffer[pImageFilePath->Length >> 1];
		while (*(p - 1) != OBJ_NAME_PATH_SEPARATOR)
		{
			p--;
		}
		l = (ULONG)(&pImageFilePath->Buffer[pImageFilePath->Length >> 1] - p);
		l *= sizeof(WCHAR);
		pFileName->MaximumLength = pFileName->Length = (USHORT)l;
		pFileName->Buffer = p;
	}
	else
	{
		pFileName->Length = pImageFilePath->Length;
		pFileName->Buffer = pImageFilePath->Buffer;
	}
    return STATUS_SUCCESS;
}

// --------------------------------------------------------------------------------------
//  

PVOID MmGetSystemRoutineAddressEx32(ULONG64 uModBase, CHAR* cSearchFnName)
{
	if (uModBase == NULL && cSearchFnName == NULL) {
		return NULL;
	}

	IMAGE_DOS_HEADER* doshdr;
	IMAGE_OPTIONAL_HEADER32* opthdr;
	IMAGE_EXPORT_DIRECTORY* pExportTable;
	ULONG* dwAddrFns, * dwAddrNames;
	USHORT* dwAddrNameOrdinals;
	ULONG dwFnOrdinal, i;
	SIZE_T uFnAddr = 0;
	char* cFunName;
	doshdr = (IMAGE_DOS_HEADER*)uModBase;
	if (NULL == doshdr)
	{
		goto __exit;
	}
	opthdr = (IMAGE_OPTIONAL_HEADER32*)(uModBase + doshdr->e_lfanew + sizeof(ULONG) + sizeof(IMAGE_FILE_HEADER));
	if (NULL == opthdr)
	{
		goto __exit;
	}
	pExportTable = (IMAGE_EXPORT_DIRECTORY*)(uModBase + opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if (NULL == pExportTable) {
		goto __exit;
	}
	dwAddrFns = (ULONG*)(uModBase + pExportTable->AddressOfFunctions);
	dwAddrNames = (ULONG*)(uModBase + pExportTable->AddressOfNames);
	dwAddrNameOrdinals = (USHORT*)(uModBase + pExportTable->AddressOfNameOrdinals);
	for (i = 0; i < pExportTable->NumberOfNames; ++i)
	{
		cFunName = (char*)(uModBase + dwAddrNames[i]);
		if (uModBase < MmUserProbeAddress)
		{
			__try {
				if (!_strnicmp(cSearchFnName, cFunName, strlen(cSearchFnName)))
				{
					if (cFunName[strlen(cSearchFnName)] == NULL)
					{
						uFnAddr = uModBase + dwAddrFns[dwAddrNameOrdinals[i]];
						break;
					}
				}
			}
			__except (1) { continue; }
		}
		else {
			if (MmIsAddressValid(cFunName) && MmIsAddressValid(cFunName + strlen(cSearchFnName)))
			{
				if (!_strnicmp(cSearchFnName, cFunName, strlen(cSearchFnName)))
				{
					if (cFunName[strlen(cSearchFnName)] == NULL)
					{
						dwFnOrdinal = pExportTable->Base + dwAddrNameOrdinals[i] - 1;
						uFnAddr = uModBase + dwAddrFns[dwFnOrdinal];
						break;
					}
				}
			}
		}
	}
__exit:
	return (PVOID)uFnAddr;
}

PVOID MmGetSystemRoutineAddressEx(ULONG64 uModBase, CHAR* cSearchFnName)
{
	if (uModBase == NULL && cSearchFnName == NULL) {
		return NULL;
	}

	IMAGE_DOS_HEADER* doshdr;
#ifdef AMD64
	IMAGE_OPTIONAL_HEADER64* opthdr;
#else
	IMAGE_OPTIONAL_HEADER32* opthdr;
#endif
	IMAGE_EXPORT_DIRECTORY* pExportTable;
	ULONG* dwAddrFns, * dwAddrNames;
	USHORT* dwAddrNameOrdinals;
	ULONG dwFnOrdinal, i;
	SIZE_T uFnAddr = 0;
	char* cFunName;
	doshdr = (IMAGE_DOS_HEADER*)uModBase;
	if (NULL == doshdr)
	{
		goto __exit;
	}
#ifdef AMD64
	opthdr = (IMAGE_OPTIONAL_HEADER64*)(uModBase + doshdr->e_lfanew + sizeof(ULONG) + sizeof(IMAGE_FILE_HEADER));
#else
	opthdr = (IMAGE_OPTIONAL_HEADER32*)(uModBase + doshdr->e_lfanew + sizeof(ULONG) + sizeof(IMAGE_FILE_HEADER));
#endif
	if (NULL == opthdr)
	{
		goto __exit;
	}
	pExportTable = (IMAGE_EXPORT_DIRECTORY*)(uModBase + opthdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if (NULL == pExportTable) {
		goto __exit;
	}
	dwAddrFns = (ULONG*)(uModBase + pExportTable->AddressOfFunctions);
	dwAddrNames = (ULONG*)(uModBase + pExportTable->AddressOfNames);
	dwAddrNameOrdinals = (USHORT*)(uModBase + pExportTable->AddressOfNameOrdinals);
	for (i = 0; i < pExportTable->NumberOfNames; ++i)
	{
		cFunName = (char*)(uModBase + dwAddrNames[i]);
		if (uModBase < MmUserProbeAddress)
		{
			__try {
				if (!_strnicmp(cSearchFnName, cFunName, strlen(cSearchFnName)))
				{
					if (cFunName[strlen(cSearchFnName)] == NULL)
					{
						uFnAddr = uModBase + dwAddrFns[dwAddrNameOrdinals[i]];
						break;
					}
				}
			}
			__except (1) { continue; }
		}
		else {
			if (MmIsAddressValid(cFunName) && MmIsAddressValid(cFunName + strlen(cSearchFnName)))
			{
				if (!_strnicmp(cSearchFnName, cFunName, strlen(cSearchFnName)))
				{
					if (cFunName[strlen(cSearchFnName)] == NULL)
					{
						dwFnOrdinal = pExportTable->Base + dwAddrNameOrdinals[i] - 1;
						uFnAddr = uModBase + dwAddrFns[dwFnOrdinal];
						break;
					}
				}
			}
		}
	}
__exit:
	return (PVOID)uFnAddr;
}

PVOID MmAllocateUserVirtualMemory(HANDLE ProcessHandle, SIZE_T AllocSize, ULONG AllocationType, ULONG Protect)
{
	PVOID Result = NULL;
	NTSTATUS(NTAPI * RtlAllocateVirtualMemory)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
	SET_PVOID(&RtlAllocateVirtualMemory, ExGetPreviousMode() == KernelMode ? NtAllocateVirtualMemory : ZwAllocateVirtualMemory);
	__try {
		RtlAllocateVirtualMemory(ProcessHandle, &Result, NULL, &AllocSize, AllocationType, Protect);
	}
	__except (1) { Result = NULL; }
	return Result;
}

NTSTATUS RtlProtectVirtualMemory(PVOID Address, SIZE_T SpaceSize, ULONG NewProtection, ULONG* OldProtect)
{
	NTSTATUS STATUS = STATUS_UNSUCCESSFUL;
	NTSTATUS(NTAPI * ZwProtectVirtualMemory)(HANDLE ProcessHandle, PVOID * BaseAddress, SIZE_T * RegionSize, ULONG NewProtectWin32, ULONG * OldProtect);
	WCHAR u_ZwQuerySection[] = { 'Z','w','Q','u','e','r','y','S','e','c','t','i','o','n', 0,0 };
	WCHAR u_ZwProtectVirtualMemory[] = { 'Z','w','P','r','o','t','e','c','t','V','i','r','t','u','a','l','M','e','m','o','r','y', 0,0 };
	UNICODE_STRING uZwQuerySection = { 0 };
	UNICODE_STRING uZwProtectVirtualMemory = { 0 };
	RtlInitUnicodeString(&uZwQuerySection, u_ZwQuerySection);
	RtlInitUnicodeString(&uZwProtectVirtualMemory, u_ZwProtectVirtualMemory);

	if ((*(PVOID*)&ZwProtectVirtualMemory = MmGetSystemRoutineAddress(&uZwProtectVirtualMemory)) == NULL) //Win7 Win8 Win8.1
	{
		UCHAR* pZwQuerySection = (UCHAR*)MmGetSystemRoutineAddress(&uZwQuerySection);
		if (pZwQuerySection != NULL)
		{
			for (size_t i = 5; i < 0x100; i++) // i=5避免直接定位到ZwQuerySection函数头
			{
				if (*(pZwQuerySection - i - 0) == 0x48 && *(pZwQuerySection - i + 1) == 0x8B && *(pZwQuerySection - i + 2) == 0xC4) //48 8B C4 Mov Rax,Rsp
				{
					*(PVOID*)&ZwProtectVirtualMemory = (pZwQuerySection - i - 0);
					break;
				}
			}
		}
	}
	if ((*(PVOID*)&ZwProtectVirtualMemory != NULL))
	{
		ULONG OffsetPreviousMode = NULL;
		UCHAR* ControlPC = (UCHAR*)ExGetPreviousMode;
		while (*ControlPC != 0xC3)
			ControlPC++;
		OffsetPreviousMode = *(ULONG*)(ControlPC - 4);

		ULONG OLD_PROTECTION = NULL;
		SIZE_T SPACE_SIZE = SpaceSize;
		PVOID PAGE_START = (PVOID)(((ULONG64)Address >> 12) << 12);

		KPROCESSOR_MODE OriMode = ExGetPreviousMode();
		*(KPROCESSOR_MODE*)((UCHAR*)PsGetCurrentThread() + OffsetPreviousMode) = UserMode;
		__try {
			STATUS = ZwProtectVirtualMemory(
				NtCurrentProcess(),
				&PAGE_START,
				&SPACE_SIZE,
				NewProtection,
				&OLD_PROTECTION
			); // 兼容COW页面
			if (NT_SUCCESS(STATUS) && OldProtect != NULL)
				*OldProtect = OLD_PROTECTION;
		}
		__except (1) { STATUS = STATUS_ACCESS_VIOLATION; }
		*(KPROCESSOR_MODE*)((UCHAR*)PsGetCurrentThread() + OffsetPreviousMode) = OriMode;
	}

	return STATUS;
}

VOID RtlSplitString(PUNICODE_STRING FullPath, PWCHAR filePath, WCHAR fileName[])
{
	PWCHAR p = FullPath->Buffer;
	int i = wcslen(p) - 1;
	int count = i;
	int j = 0;
	while (p[i] != '\\')
	{
		fileName[j] = p[i];
		i--;
		j++;
	}
	WCHAR tmp;
	int k = 0;
	int lenght = wcslen(fileName);
	for (j = lenght - 1; k < j; k++, j--)
	{

		tmp = fileName[k];
		fileName[k] = fileName[j];
		fileName[j] = tmp;
	}
	j = 0;
	for (i = 0; i < count - lenght; i++)
	{
		filePath[j] = p[i];
		j++;
	}
}

VOID RtldelWchar(wchar_t* str, wchar_t* ch)
{
	wchar_t* pstr = pstr = wcsstr(str, ch);
	if (NULL == pstr)
	{
		return;
	}
	else
	{
		for (pstr; pstr != NULL; pstr = wcsstr(pstr, ch))
		{
			wchar_t* temp = pstr;
			wchar_t* ptemp = pstr;
			ptemp += wcslen(ch);
			while (*temp != L'\0')
			{
				*temp = *ptemp;
				temp++;
				ptemp++;
			}
		}
	}
}