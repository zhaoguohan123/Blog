#include "ProcessProtect.h"

HHOOK g_hHook = NULL;
HWND g_hTaskManager = NULL;
TCHAR g_szTargetProcessName[MAX_PATH] = _T(""); // 目标进程的进程名

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_CREATEWND) {
        HWND hwnd = (HWND)wParam;
        TCHAR className[256];
        GetClassName(hwnd, className, 256);

        // 检查是否是任务管理器窗口
        if (_tcscmp(className, _T("TaskManagerWindowClass")) == 0) {
            g_hTaskManager = hwnd;
        }
    }
    std::cerr << "nCode: " << nCode << std::endl;
    // 处理结束进程消息
    if (nCode == HCBT_SYSCOMMAND && wParam == SC_CLOSE && g_hTaskManager != NULL) {
        // 获取要结束的进程的窗口句柄
        HWND hwndTarget = (HWND)lParam;
        DWORD dwProcessId;
        GetWindowThreadProcessId(hwndTarget, &dwProcessId);

        // 获取进程名
        TCHAR szProcessName[MAX_PATH];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
        if (hProcess != NULL)
        {
            if (GetModuleFileNameEx(hProcess, NULL, szProcessName, MAX_PATH))
            {
                // 检查是否是目标进程
                if (_tcsicmp(szProcessName, g_szTargetProcessName) == 0)
                {
                    // 拦截结束进程的消息
                    std::cout << "Ending process for the target process is intercepted!" << std::endl;
                    CloseHandle(hProcess);
                    return 1; // 阻止默认操作
                }
            }
            CloseHandle(hProcess);
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int ProcessProtectTest() {
    // 设置目标进程的进程名
    _tcscpy_s(g_szTargetProcessName, _T("notepad++.exe")); // 替换成你要拦截的目标进程的进程名

    // 安装全局钩子
    g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL,0);
    if (g_hHook == NULL) {
        std::cerr << "Failed to install hook!" << "errcode: " << GetLastError() << std::endl;
        return 1;
    }

    // 进入消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 卸载钩子
    UnhookWindowsHookEx(g_hHook);

    return 0;
}