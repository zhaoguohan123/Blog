#pragma once
#include <ntifs.h>
#include <ntimage.h>
#include <intrin.h>
#include <minwindef.h>

#pragma pack(push,1)
typedef struct _SHELL_CODE
{
	UCHAR Head[0x1E];
	USHORT Mov_Rcx_Immed;
	PWCHAR PathToFile;
	USHORT Mov_Rdx_Immed;
	ULONG* Flags;
	USHORT Mov_R8_Immed;
	PVOID ModuleFileName;
	USHORT Mov_R9_Immed;
	PHANDLE ModuleHanlde;
	UCHAR Call_LdrLoadDll[0x0B];
	USHORT Mov_Rsi_Immed;
	PVOID movsb_Source;
	USHORT Mov_Rdi_Immed;
	PVOID movsb_Destance;
	USHORT Mov_Rcx_Immed_02;
	SIZE_T movsb_Size;
	USHORT rep_movsb;
	UCHAR End[0x25];
	PVOID pfn_LdrLoadDll;
	PVOID pfn_ZwContinue;
	UCHAR tmp_ZwContinue_Code[0x0C];
	WCHAR tmp_DllPath[256];
	ULONG tmp_Flags;
	UNICODE_STRING64 tmp_ustr;
	HANDLE tmp_ModuleHanlde;
}SHELL_CODE, * PSHELL_CODE;

typedef struct _JUMP_CODE
{
	USHORT Mov_Rax_Immed;
	PVOID Address;
	USHORT Jump_Rax;
}JUMP_CODE, * PJUMP_CODE;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct _SHELL_CODE32
{
	UCHAR Pushad;
	UCHAR Push_Immed_01;
	ULONG32 ModuleHanlde;
	UCHAR Push_Immed_02;
	ULONG32 ModuleFileName;
	UCHAR Push_Immed_03;
	ULONG32 Flags;
	UCHAR Push_Immed_04;
	ULONG32 PathToFile;
	UCHAR Mov_Eax_Immed;
	ULONG32 pfn_LdrLoadDll;
	USHORT Call_Eax;
	ULONG Mov_pEsp_1C_Eax;
	UCHAR Mov_Esi_Immed;
	ULONG32 movsb_Source;
	UCHAR Mov_Edi_Immed;
	ULONG32 movsb_Destance;
	UCHAR Mov_Ecx_Immed_02;
	ULONG32 movsb_Size;
	USHORT rep_movsb;
	UCHAR popad;
	UCHAR Mov_Eax_Immed_2;
	ULONG32 pfn_ZwContinue;
	USHORT Jmp_Eax;
	UCHAR tmp_ZwContinue_Code[0x07];
	WCHAR tmp_DllPath[256];
	ULONG tmp_Flags;
	UNICODE_STRING32 tmp_ustr;
	HANDLE tmp_ModuleHanlde;
}SHELL_CODE32, * PSHELL_CODE32;

typedef struct _JUMP_CODE32
{
	UCHAR Mov_Eax_Immed;
	ULONG32 Address;
	USHORT Jump_Eax;
}JUMP_CODE32, * PJUMP_CODE32;
#pragma pack(pop)

// ----------------------- 
// 只在ProcessProhibition中使用

NTSTATUS InitProcessInjection(PDRIVER_OBJECT pDrv);
VOID UnInitProcessInjection(PDRIVER_OBJECT pDrv);