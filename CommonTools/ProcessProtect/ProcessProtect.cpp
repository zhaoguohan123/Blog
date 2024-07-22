#include "ProcessProtect.h"

HHOOK g_hHook = NULL;
HWND g_hTaskManager = NULL;
TCHAR g_szTargetProcessName[MAX_PATH] = _T(""); // Ŀ����̵Ľ�����

LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_CREATEWND) {
        HWND hwnd = (HWND)wParam;
        TCHAR className[256];
        GetClassName(hwnd, className, 256);

        // ����Ƿ����������������
        if (_tcscmp(className, _T("TaskManagerWindowClass")) == 0) {
            g_hTaskManager = hwnd;
        }
    }
    std::cerr << "nCode: " << nCode << std::endl;
    // �������������Ϣ
    if (nCode == HCBT_SYSCOMMAND && wParam == SC_CLOSE && g_hTaskManager != NULL) {
        // ��ȡҪ�����Ľ��̵Ĵ��ھ��
        HWND hwndTarget = (HWND)lParam;
        DWORD dwProcessId;
        GetWindowThreadProcessId(hwndTarget, &dwProcessId);

        // ��ȡ������
        TCHAR szProcessName[MAX_PATH];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId);
        if (hProcess != NULL)
        {
            if (GetModuleFileNameEx(hProcess, NULL, szProcessName, MAX_PATH))
            {
                // ����Ƿ���Ŀ�����
                if (_tcsicmp(szProcessName, g_szTargetProcessName) == 0)
                {
                    // ���ؽ������̵���Ϣ
                    std::cout << "Ending process for the target process is intercepted!" << std::endl;
                    CloseHandle(hProcess);
                    return 1; // ��ֹĬ�ϲ���
                }
            }
            CloseHandle(hProcess);
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

int ProcessProtectTest() {
    // ����Ŀ����̵Ľ�����
    _tcscpy_s(g_szTargetProcessName, _T("notepad++.exe")); // �滻����Ҫ���ص�Ŀ����̵Ľ�����

    // ��װȫ�ֹ���
    g_hHook = SetWindowsHookEx(WH_CBT, CBTProc, NULL,0);
    if (g_hHook == NULL) {
        std::cerr << "Failed to install hook!" << "errcode: " << GetLastError() << std::endl;
        return 1;
    }

    // ������Ϣѭ��
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // ж�ع���
    UnhookWindowsHookEx(g_hHook);

    return 0;
}