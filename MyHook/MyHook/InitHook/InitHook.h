#pragma once
#include "utils.h"
typedef BOOL (__stdcall * PFUNC_SETHOOKS)(void);
typedef BOOL(__stdcall * PFUNC_REMOVEHOOKS)(void);

#define HOOK_NAMEA "MyHook.dll"
#define HOOK_NAMEW UNICODETOANSI(HOOK_NAMEA)

/*��װ����*/
BOOL InstallHook(void);

/*ж�ع���*/
BOOL UnInstallHook();

