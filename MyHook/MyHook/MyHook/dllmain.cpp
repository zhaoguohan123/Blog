// dllmain.cpp : ���� DLL Ӧ�ó������ڵ㡣
#include "stdafx.h"
#define HOOK_NAMEA	"MyHook.dll"

HINSTANCE g_hHookInstance = NULL;
DWORD g_dwTlsInd = 0;
HHOOK  g_hCallWndHook = NULL;
HHOOK  g_hKeyboardHook = NULL;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			LoadLibraryA(HOOK_NAMEA);
			g_hHookInstance = hModule;
			g_dwTlsInd = TlsAlloc(); // �ֲ߳̾��洢��
			if (g_dwTlsInd == TLS_OUT_OF_INDEXES)
			{
				DebugPrintA("TlsAlloc failed!!,errcode = %u", GetLastError());
			}
			//InstallApiHook();
			break;
		}
		case DLL_THREAD_ATTACH:
			break;
		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
		{
			if (g_dwTlsInd != TLS_OUT_OF_INDEXES)
			{
				TlsFree(g_dwTlsInd);
			}
			//UnInstallApiHook();
			break;
		}
		default:
			break;
	}
	return TRUE;
}

void DebugPrintA(const char* format, ...)
{
	if (format == NULL)
	{
		return;
	}
	char buffer[MAX_LOG_LEN] = { 0 };

	va_list ap;
	va_start(ap, format);
	(void)StringCchVPrintfA(buffer, _countof(buffer), format, ap);
	va_end(ap);

	StringCchCatA(buffer, MAX_LOG_LEN, "[cooper]");
	OutputDebugStringA(buffer);
}

// ��װ���̹��Ӻʹ�����Ϣ����
extern "C" BOOL WINAPI SetHooks() 
{
	BOOL bSuccess = TRUE;

	if (!g_hCallWndHook)
	{
		g_hCallWndHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hHookInstance, 0);  
		// ������SetHooks������ʱ��SetWindowsHookEx�����ͽ�CallWndProc��ӵ�����
		if (g_hCallWndHook == NULL)
		{
			DebugPrintA("SetHooks CallWndProc failed: %d", GetLastError());
			bSuccess = FALSE;
		}
	}

	if (!g_hKeyboardHook)
	{
		g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hHookInstance, 0);                   // ���̹���
		if (g_hKeyboardHook == NULL)
		{
			DebugPrintA("SetHooks LowLevelKeyboardProc failed: %d", GetLastError());
			bSuccess = FALSE;
		}
	}

	if (!bSuccess)
	{
		RemoveHooks();
	}
	return bSuccess;
}

// ж�ؼ��̹��Ӻʹ��ڹ���
extern "C" BOOL WINAPI RemoveHooks()
{
	if (g_hCallWndHook)
	{
		if (!UnhookWindowsHookEx(g_hCallWndHook))
		{
			DebugPrintA("Failed to UnhookWindowsHookEx g_hCallWndHook,error code = %u.",
				GetLastError());
		}
		g_hCallWndHook = NULL;
	}

	if (g_hKeyboardHook)
	{
		if (!UnhookWindowsHookEx(g_hKeyboardHook))
		{
			DebugPrintA("Failed to UnhookWindowsHookEx g_hKeyboardHook,error code = %u.",
				GetLastError());
		}
		g_hKeyboardHook = NULL;
	}
	return TRUE;
}

// ���ڹ��ӻص�
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) 
{
	
	if (nCode < 0)
	{
		return CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
	}
	char szPath[MAX_PATH] = { 0, };        //MAX_PATH�����˱�������֧�ֵ��ȫ·�����ĳ���
	char *p = NULL;
	GetModuleFileNameA(NULL, szPath, MAX_PATH);   //��ǰ���̵�ȫ·�����浽szPath��
	p = strrchr(szPath, '\\');     //pΪ�ַ�'\\'֮��szPath�ַ����Ľ�β,˵��ȡ���ǵ�ǰ����Ľ�����

	if (0 == _stricmp(p + 1,"Dbgview.exe")) // DebugPrintAҲ���debugview��������Ϣ�����Կ��ܻᵼ��debivie.exe��������Ӧ
	{
		return CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
	}
	
	if (0 == _stricmp(p + 1, "notepad.exe")) // �Ƚϵ�ǰ�������ƣ���Ϊnotepad.exe������Ϣ���ᴫ�ݸ�Ӧ�ó������һ�����Ӻ���
	{
		CWPSTRUCT* pMsgParam = (CWPSTRUCT*)lParam;
		HWND hwnd = pMsgParam->hwnd;

		switch (pMsgParam->message)
		{
			case WM_SHOWWINDOW:
			{
				DebugPrintA("HOOK WM_SHOWWINDOW ");
			}
			break;

			case WM_SIZE:
			{
				DebugPrintA("HOOK WM_SIZE");
			}
			break;
		default:
			break;
		}
	}

	return CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
}


// ���̹��ӻص�
LRESULT CALLBACK LowLevelKeyboardProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	//��WH_KEYBOARD_LLģʽ��lParam ��ָ��KBDLLHOOKSTRUCT���͵�ַ
	KBDLLHOOKSTRUCT *pkbhs = (KBDLLHOOKSTRUCT *)lParam;
	//���nCode����HC_ACTION�������Ϣ�����С��0�������ӳ̾ͱ��뽫����Ϣ���ݸ� CallNextHookEx
	//if (nCode == HC_ACTION){
	if (pkbhs->vkCode == VK_ESCAPE && GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState(VK_SHIFT) & 0x8000) {
		DebugPrintA("Ctrl+Shift+Esc"); // �����������ݼ�
		return 1;
	}
	else if (pkbhs->vkCode == VK_TAB && pkbhs->flags & LLKHF_ALTDOWN) {
		DebugPrintA("Alt+Tab");
		return 1;
	}
	else if (pkbhs->vkCode == VK_LWIN || pkbhs->vkCode == VK_RWIN) {
		DebugPrintA("LWIN/RWIN");
		return 1;
	}

	return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}