#include "ProcessInjection.h"
#include "util.h"
#include "InjectList.h"
#include "ProcessProhibition.h"
#include <ntstrsafe.h>

#define TagDllName L"\\TroilaHook.dll"
#define TagDllName32  L"\\TroilaHook32.dll"

WCHAR* DllPatch = NULL;
WCHAR* DllPatch32 = NULL;
WCHAR FilePath[MAX_PATH] = { 0 };
WCHAR FileName[MAX_PATH] = { 0 };
WCHAR FilePath32[MAX_PATH] = { 0 };
WCHAR FileName32[MAX_PATH] = { 0 };

// 32位注入功能shellcode
PVOID RtlMakeLdrLoadDllCall32(CONST WCHAR* lpDllPath, PVOID LdrLoadDllAddress, PVOID ZwContinueAddress)
{
	SHELL_CODE32 ShellCode32 = { 0 };
	ShellCode32.Pushad = 0x60;
	ShellCode32.Push_Immed_01 = 0x68;
	ShellCode32.Push_Immed_02 = 0x68;
	ShellCode32.Push_Immed_03 = 0x68;
	ShellCode32.Push_Immed_04 = 0x68;
	ShellCode32.Mov_Eax_Immed = 0xB8;
	ShellCode32.Call_Eax = 0xD0FF;
	ShellCode32.Mov_pEsp_1C_Eax = 0x1C244489;
	ShellCode32.Mov_Esi_Immed = 0xBE;
	ShellCode32.Mov_Edi_Immed = 0xBF;
	ShellCode32.Mov_Ecx_Immed_02 = 0xB9;
	ShellCode32.rep_movsb = 0xA4F3;
	ShellCode32.popad = 0x61;
	ShellCode32.Mov_Eax_Immed_2 = 0xB8;
	ShellCode32.Jmp_Eax = 0xE0FF;

	JUMP_CODE32 jumpCode32 = { 0 };
	jumpCode32.Mov_Eax_Immed = 0xB8;
	jumpCode32.Jump_Eax = 0xE0FF;

	PVOID Result = NULL;
	if (NT_SUCCESS(RtlProtectVirtualMemory(ZwContinueAddress, PAGE_SIZE, PAGE_EXECUTE_READWRITE, NULL)))
	{
		PVOID K32_LdrLoadDll = MmAllocateUserVirtualMemory(NtCurrentProcess(), sizeof(ShellCode32), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (K32_LdrLoadDll != NULL)
		{
			RtlMoveMemory(K32_LdrLoadDll, &ShellCode32.Pushad, sizeof(ShellCode32));
			RtlMoveMemory(((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ZwContinue_Code, ZwContinueAddress, sizeof(ShellCode32.tmp_ZwContinue_Code));

			wcscat(((PSHELL_CODE32)K32_LdrLoadDll)->tmp_DllPath, lpDllPath);

			((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ustr.Buffer = (ULONG64)(&(((PSHELL_CODE32)K32_LdrLoadDll)->tmp_DllPath[0]));
			((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ustr.Length = wcslen((WCHAR*)lpDllPath) * 2;
			((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ustr.MaximumLength = ((((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ustr.Length / 2) + 1) * 2;

			((PSHELL_CODE32)K32_LdrLoadDll)->pfn_LdrLoadDll = (ULONG32)LdrLoadDllAddress;
			((PSHELL_CODE32)K32_LdrLoadDll)->pfn_ZwContinue = (ULONG32)ZwContinueAddress;
			((PSHELL_CODE32)K32_LdrLoadDll)->movsb_Destance = (ULONG32)ZwContinueAddress;
			((PSHELL_CODE32)K32_LdrLoadDll)->movsb_Source = (ULONG32)((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ZwContinue_Code;
			((PSHELL_CODE32)K32_LdrLoadDll)->movsb_Size = sizeof(ShellCode32.tmp_ZwContinue_Code);

			((PSHELL_CODE32)K32_LdrLoadDll)->PathToFile = NULL;
			((PSHELL_CODE32)K32_LdrLoadDll)->Flags = (ULONG32) & ((PSHELL_CODE32)K32_LdrLoadDll)->tmp_Flags;
			((PSHELL_CODE32)K32_LdrLoadDll)->ModuleFileName = (ULONG32) & ((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ustr;
			((PSHELL_CODE32)K32_LdrLoadDll)->ModuleHanlde = (ULONG32) & ((PSHELL_CODE32)K32_LdrLoadDll)->tmp_ModuleHanlde;

			jumpCode32.Address = (ULONG32)K32_LdrLoadDll;
			RtlMoveMemory(ZwContinueAddress, &jumpCode32.Mov_Eax_Immed, sizeof(jumpCode32));
			Result = K32_LdrLoadDll;
		}
	}
	return Result;
}

// 64位注入功能shellcode
PVOID RtlMakeLdrLoadDllCall64(CONST WCHAR* lpDllPath, PVOID LdrLoadDllAddress, PVOID ZwContinueAddress)
{
	UCHAR head[0x1E] = {
		0x50, 0x53, 0x51, 0x52, 0x55, 0x6A, 0x00, 0x56, 0x57, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41,
		0x53, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x90, 0x48, 0x83, 0xEC, 0x68
	};
	UCHAR callLdrLoadDll[0x0B] = {
		0xFF, 0x15, 0x4A, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x68, 0x90
	};
	UCHAR end[0x25] = {
		0x90, 0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41,
		0x58, 0x5F, 0x5E, 0x48, 0x83, 0xC4, 0x08, 0x5D, 0x5A, 0x59, 0x5B, 0x48, 0x83, 0xC4, 0x08, 0xFF,
		0x25, 0x08, 0x00, 0x00, 0x00
	};

	SHELL_CODE ShellCode = { 0 };
	memcpy(ShellCode.Head, head, sizeof(head));
	memcpy(ShellCode.Call_LdrLoadDll, callLdrLoadDll, sizeof(callLdrLoadDll));
	memcpy(ShellCode.End, end, sizeof(end));
	ShellCode.Mov_Rcx_Immed = 0xB948;
	ShellCode.Mov_Rdx_Immed = 0xBA48;
	ShellCode.Mov_R8_Immed = 0xB849;
	ShellCode.Mov_R9_Immed = 0xB949;
	ShellCode.Mov_Rsi_Immed = 0xBE48;
	ShellCode.Mov_Rdi_Immed = 0xBF48;
	ShellCode.Mov_Rcx_Immed_02 = 0xB948;
	ShellCode.rep_movsb = 0xA4F3;

	JUMP_CODE jumpCode = { 0 };
	jumpCode.Mov_Rax_Immed = 0xB848;
	jumpCode.Jump_Rax = 0xE0FF;

	PVOID Result = NULL;
	if (NT_SUCCESS(RtlProtectVirtualMemory(ZwContinueAddress, PAGE_SIZE, PAGE_EXECUTE_READWRITE, NULL)))
	{
		PVOID K64_LdrLoadDll = MmAllocateUserVirtualMemory(NtCurrentProcess(), sizeof(SHELL_CODE), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (K64_LdrLoadDll != NULL)
		{
			RtlMoveMemory(K64_LdrLoadDll, &ShellCode.Head[0], sizeof(ShellCode));
			RtlMoveMemory(((PSHELL_CODE)K64_LdrLoadDll)->tmp_ZwContinue_Code, ZwContinueAddress, sizeof(ShellCode.tmp_ZwContinue_Code));
			wcscat(((PSHELL_CODE)K64_LdrLoadDll)->tmp_DllPath, lpDllPath);
			((PSHELL_CODE)K64_LdrLoadDll)->tmp_ustr.Buffer = (ULONG64)(&(((PSHELL_CODE)K64_LdrLoadDll)->tmp_DllPath[0]));
			((PSHELL_CODE)K64_LdrLoadDll)->tmp_ustr.Length = wcslen((WCHAR*)lpDllPath) * 2;
			((PSHELL_CODE)K64_LdrLoadDll)->tmp_ustr.MaximumLength = ((((PSHELL_CODE)K64_LdrLoadDll)->tmp_ustr.Length / 2) + 1) * 2;

			((PSHELL_CODE)K64_LdrLoadDll)->pfn_LdrLoadDll = LdrLoadDllAddress;
			((PSHELL_CODE)K64_LdrLoadDll)->pfn_ZwContinue = ZwContinueAddress;
			((PSHELL_CODE)K64_LdrLoadDll)->movsb_Destance = ZwContinueAddress;
			((PSHELL_CODE)K64_LdrLoadDll)->movsb_Source = ((PSHELL_CODE)K64_LdrLoadDll)->tmp_ZwContinue_Code;
			((PSHELL_CODE)K64_LdrLoadDll)->movsb_Size = sizeof(ShellCode.tmp_ZwContinue_Code);

			((PSHELL_CODE)K64_LdrLoadDll)->PathToFile = NULL;
			((PSHELL_CODE)K64_LdrLoadDll)->Flags = &((PSHELL_CODE)K64_LdrLoadDll)->tmp_Flags;
			((PSHELL_CODE)K64_LdrLoadDll)->ModuleFileName = &((PSHELL_CODE)K64_LdrLoadDll)->tmp_ustr;
			((PSHELL_CODE)K64_LdrLoadDll)->ModuleHanlde = &((PSHELL_CODE)K64_LdrLoadDll)->tmp_ModuleHanlde;

			jumpCode.Address = K64_LdrLoadDll;
			RtlMoveMemory(ZwContinueAddress, &jumpCode.Mov_Rax_Immed, sizeof(jumpCode));
			Result = K64_LdrLoadDll;
		}
	}
	return Result;
	/*
	50 53 51 52 55 6A 00 56 57 41 50 41 51 41 52 41 53 41 54 41 55 41 56 41 57 90 48 83 EC 68 48 B9
	AA AA AA AA AA AA AA AA 48 BA AA AA AA AA AA AA AA AA 49 B8 AA AA AA AA AA AA AA AA 49 B9 AA AA
	AA AA AA AA AA AA FF 15 24 00 00 00 90 48 83 C4 68 41 5F 41 5E 41 5D 41 5C 41 5B 41 5A 41 59 41
	58 5F 5E 48 83 C4 08 5D 5A 59 5B 48 83 C4 08 C3
	*/
}

// 模块回调
VOID CbLoadImage(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo)
{
	NTSTATUS status;
	ULONG oldProtection;

	if ((ULONG64)ProcessId > 0x04 && ImageInfo && ImageInfo->SystemModeImage == FALSE && KeGetCurrentIrql() < DISPATCH_LEVEL/*&& ProcessRuntimeStat.IsInject == TRUE*/ )
	{
		if (NULL == PsGetProcessWow64Process(PsGetCurrentProcess())) // x64
		{
			if (FullImageName && FullImageName->Buffer && FullImageName->Length && wcslen(FullImageName->Buffer) >= (sizeof("System32\\ntdll.dll") - 1))
			{
				WCHAR pszDest[MAX_PATH] = { 0 };
				if (STATUS_SUCCESS != RtlStringCbCopyW(pszDest, MAX_PATH, FullImageName->Buffer))
				{
					return;
				}
				if (wcsstr(pszDest, L"System32\\ntdll.dll"))
				{
					// 已注入或不在注入表中
					if (PsQueryInjectListStatus(ProcessId))
					{
						return;
					}
					PVOID pfn_ZwContinue = MmGetSystemRoutineAddressEx((ULONG64)ImageInfo->ImageBase, "ZwContinue");
					PVOID pfn_LdrLoadDll = MmGetSystemRoutineAddressEx((ULONG64)ImageInfo->ImageBase, "LdrLoadDll");
					if (NULL != RtlMakeLdrLoadDllCall64(DllPatch, pfn_LdrLoadDll, pfn_ZwContinue))
					{
						PsSetInjectListStatus(ProcessId);
					}
				}
			}
		}
		else
		{
			if (FullImageName && FullImageName->Buffer && FullImageName->Length && wcslen(FullImageName->Buffer) >= (sizeof("SysWOW64\\ntdll.dll") - 1))
			{
				WCHAR pszDest[MAX_PATH] = { 0 };
				if (STATUS_SUCCESS != RtlStringCbCopyW(pszDest, MAX_PATH, FullImageName->Buffer))
				{
					return;
				}
				if (wcsstr(pszDest, L"SysWOW64\\ntdll.dll"))
				{
					if (PsQueryInjectListStatus(ProcessId))
					{
						return;
					}
					PVOID pfn_ZwContinue = MmGetSystemRoutineAddressEx32((ULONG64)ImageInfo->ImageBase, "ZwContinue");
					PVOID pfn_LdrLoadDll = MmGetSystemRoutineAddressEx32((ULONG64)ImageInfo->ImageBase, "LdrLoadDll");
					if (NULL != RtlMakeLdrLoadDllCall32(DllPatch32, pfn_LdrLoadDll, pfn_ZwContinue))
					{
						PsSetInjectListStatus(ProcessId);
					}
				}
			}
		}
	}
}

// 初始化进程注入
NTSTATUS InitProcessInjection(PDRIVER_OBJECT pDrv)
{
	NTSTATUS stat = STATUS_SUCCESS;

	PKLDR_DATA_TABLE_ENTRY64 pldt = (PKLDR_DATA_TABLE_ENTRY64)pDrv->DriverSection;
	UNICODE_STRING DriverPath = pldt->FullDllName;
	UNICODE_STRING DriverPath32 = pldt->FullDllName;
	UNICODE_STRING DriverBaseName = pldt->BaseDllName;

	DllPatch = (PWCHAR)ExAllocatePool(NonPagedPool, MAX_PATH);
	DllPatch32 = (PWCHAR)ExAllocatePool(NonPagedPool, MAX_PATH);
	if (DllPatch == NULL || DllPatch32 == NULL)
	{
		stat = STATUS_UNSUCCESSFUL;
		goto end;
	}
	if (!MmIsAddressValid(DllPatch) || !MmIsAddressValid(DllPatch32))
	{
		stat = STATUS_UNSUCCESSFUL;
		goto end;
	}

	RtlZeroMemory(DllPatch, MAX_PATH);
	RtlZeroMemory(DllPatch32, MAX_PATH);
	
	RtlSplitString(&DriverPath, FilePath, FileName);
	RtlSplitString(&DriverPath32, FilePath32, FileName32);

	DllPatch = wcscat(FilePath, TagDllName);
	DllPatch32 = wcscat(FilePath32, TagDllName32);

	RtldelWchar(DllPatch, L"\\??\\");
	RtldelWchar(DllPatch32, L"\\??\\");

	// 模块回调
	ULONG oldFlags = RtlFakeCallBackVerify(pDrv->DriverSection);
	stat = PsSetLoadImageNotifyRoutine(CbLoadImage);
	RtlResetCallBackVerify(pDrv->DriverSection, oldFlags);

end:
	return stat;
}

// 卸载
VOID UnInitProcessInjection(PDRIVER_OBJECT pDrv)
{
	ULONG oldFlags = RtlFakeCallBackVerify(pDrv->DriverSection);
	PsRemoveLoadImageNotifyRoutine(CbLoadImage);
	RtlResetCallBackVerify(pDrv->DriverSection, oldFlags);
}