#pragma once
#include <ntifs.h>
#include <minwindef.h>
#include "Unrevealed.h"

ULONG RtlFakeCallBackVerify(PVOID ldr);
VOID RtlResetCallBackVerify(PVOID ldr, ULONG oldFlags);
NTSTATUS RtlGetFileName(const PUNICODE_STRING pImageFilePath, PUNICODE_STRING pFileName);	// PASSIVE_LEVEL


NTSTATUS RtlProtectVirtualMemory(PVOID Address, SIZE_T SpaceSize, ULONG NewProtection, ULONG* OldProtect);
PVOID MmAllocateUserVirtualMemory(HANDLE ProcessHandle, SIZE_T AllocSize, ULONG AllocationType, ULONG Protect);
PVOID MmGetSystemRoutineAddressEx(ULONG64 uModBase, CHAR* cSearchFnName);
PVOID MmGetSystemRoutineAddressEx32(ULONG64 uModBase, CHAR* cSearchFnName);

VOID RtlSplitString(PUNICODE_STRING FullPath, PWCHAR filePath, WCHAR fileName[]);
VOID RtldelWchar(wchar_t* str, wchar_t* ch);