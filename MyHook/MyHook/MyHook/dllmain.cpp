// dllmain.cpp : 定义 DLL 应用程序的入口点。
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
			g_dwTlsInd = TlsAlloc(); // 线程局部存储区
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

extern "C" BOOL WINAPI SetHooks() 
{
	BOOL bSuccess = TRUE;

	if (!g_hCallWndHook)
	{
		g_hCallWndHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hHookInstance, 0);                             // 消息钩子
		if (g_hCallWndHook == NULL)
		{
			DebugPrintA("SetHooks CallWndProc failed: %d", GetLastError());
			bSuccess = FALSE;
		}
	}

	if (!g_hKeyboardHook)
	{
		g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hHookInstance, 0);                   // 键盘钩子
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

LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) 
{
	if (nCode < 0)
	{
		return CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
	}
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
	CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
}


LRESULT CALLBACK LowLevelKeyboardProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	KBDLLHOOKSTRUCT *Key_Info = (KBDLLHOOKSTRUCT*)lParam;
	if (HC_ACTION == nCode)
	{
		if (WM_KEYDOWN == wParam || WM_SYSKEYDOWN) 
		{
			if (Key_Info->vkCode == 'P'&& (GetKeyState(VK_LWIN) & 0x8000) ) //屏敝 WIN+P 组合键(左)
			{
				return TRUE;
			}
		}
	}
	return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}