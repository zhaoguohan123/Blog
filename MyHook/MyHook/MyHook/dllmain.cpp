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

// 安装键盘钩子和窗口消息钩子
extern "C" BOOL WINAPI SetHooks() 
{
	BOOL bSuccess = TRUE;

	if (!g_hCallWndHook)
	{
		g_hCallWndHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, g_hHookInstance, 0);  
		// 当调用SetHooks函数的时候，SetWindowsHookEx函数就将CallWndProc添加到钩链
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

// 卸载键盘钩子和窗口钩子
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

// 窗口钩子回调
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam) 
{
	
	if (nCode < 0)
	{
		return CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
	}
	char szPath[MAX_PATH] = { 0, };        //MAX_PATH定义了编译器所支持的最长全路径名的长度
	char *p = NULL;
	GetModuleFileNameA(NULL, szPath, MAX_PATH);   //当前进程的全路径储存到szPath中
	p = strrchr(szPath, '\\');     //p为字符'\\'之后到szPath字符串的结尾,说明取的是当前程序的进程名

	if (0 == _stricmp(p + 1,"Dbgview.exe")) // DebugPrintA也会给debugview发窗口消息，所以可能会导致debivie.exe窗口无响应
	{
		return CallNextHookEx(g_hCallWndHook, nCode, wParam, lParam);
	}
	
	if (0 == _stricmp(p + 1, "notepad.exe")) // 比较当前进程名称，若为notepad.exe，则消息不会传递给应用程序或下一个钩子函数
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


// 键盘钩子回调
LRESULT CALLBACK LowLevelKeyboardProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	//在WH_KEYBOARD_LL模式下lParam 是指向KBDLLHOOKSTRUCT类型地址
	KBDLLHOOKSTRUCT *pkbhs = (KBDLLHOOKSTRUCT *)lParam;
	//如果nCode等于HC_ACTION则处理该消息，如果小于0，则钩子子程就必须将该消息传递给 CallNextHookEx
	//if (nCode == HC_ACTION){
	if (pkbhs->vkCode == VK_ESCAPE && GetAsyncKeyState(VK_CONTROL) & 0x8000 && GetAsyncKeyState(VK_SHIFT) & 0x8000) {
		DebugPrintA("Ctrl+Shift+Esc"); // 任务管理器快捷键
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