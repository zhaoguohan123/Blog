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
	pfnSetHook(); // ִ�йҹ��Ĳ���
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

	MSG msg;
	while (1)      //����©����Ϣ������Ȼ����Ῠ��
	{
		if (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else
			Sleep(0);    //����CPUȫ��������
	}

	UnInstallHook();
	return 0;
}