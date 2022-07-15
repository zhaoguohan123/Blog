#include "InitHook.h"


HMODULE s_hHookMod = NULL;



BOOL InstallHook()
{
	BOOL bRet = FALSE;

	PFUNC_SETHOOKS pfnSetHook = NULL;

	s_hHookMod = LoadLibraryA(HOOK_NAMEA);
	if (NULL == s_hHookMod)
	{
		DebugPrintA("load hook failed, errcode = %u!!!", GetLastError());
		goto ExitFunc;
	}

	pfnSetHook = (PFUNC_SETHOOKS)GetProcAddress(s_hHookMod, "SetHooks");
	if (NULL == pfnSetHook)
	{
		DebugPrintA("get sethook failed, errcode = %u!!!", GetLastError());
		goto ExitFunc;
	}
	pfnSetHook(); // Ö´ÐÐ¹Ò¹³µÄ²Ù×÷
	bRet = TRUE;
ExitFunc:
	if (FALSE == bRet)
	{
		UnInstallHook();
	}
	return  bRet;
}

BOOL UnInstallHook()
{
	if (s_hHookMod != NULL)
	{
		PFUNC_REMOVEHOOKS pfnRemoveHooks = (PFUNC_REMOVEHOOKS)GetProcAddress(s_hHookMod, "RemoveHooks");
		if (pfnRemoveHooks != NULL)
		{
			pfnRemoveHooks();
		}
		if (s_hHookMod)
		{
			FreeLibrary(s_hHookMod);
		}
		s_hHookMod = NULL;
	}
	return TRUE;
}


int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPreInstance,
	LPTSTR lpCmd,
	int nCmd)

{
	if (FALSE == InstallHook())
	{
		DebugPrintA("InstallHook failed!!");
	}

	do 
	{
		Sleep(5000);
	} while (TRUE);

	UnInstallHook();
}