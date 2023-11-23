#ifndef _HIDE_TRAY_ICON_HEAD_
#define _HIDE_TRAY_ICON_HEAD_
#include "CommonHead.h"

namespace HIDE_TRAY_ICON {
    void DeleteTrayIcon()
    {
        while (true)
        {

            //MyDebugPrintW(L"---------------------begin-----------------------------------------------");
            bool failed = false;
            LRESULT dwButtonCount = 0;
            DWORD dwProcessId = 0;
            HANDLE hProcess = NULL;
            LPVOID lpAddress = NULL;
            TBBUTTON tb;

            do
            {
                HWND hTrayWnd = FindWindow(L"Shell_TrayWnd", NULL);
                if (hTrayWnd == NULL)
                {
                    failed = true;
                    MyDebugPrintW(L"hTrayWnd is null, getlasterror:%u", GetLastError());
                    break;
                }

                HWND hNotifyWnd = FindWindowEx(hTrayWnd, NULL, L"TrayNotifyWnd", NULL);
                if (hNotifyWnd == NULL)
                {
                    failed = true;
                    MyDebugPrintW(L"hNotifyWnd is null, getlasterror:%u", GetLastError());
                    break;
                }

                HWND hSysPagerWnd = FindWindowEx(hNotifyWnd, NULL, L"SysPager", NULL);
                if (hSysPagerWnd == NULL)
                {
                    failed = true;
                    MyDebugPrintW(L"hSysPagerWnd is null, getlasterror:%u", GetLastError());
                    break;
                }

                HWND hToolbar = FindWindowEx(hSysPagerWnd, NULL, L"ToolbarWindow32", NULL);
                if (hToolbar == NULL)
                {
                    failed = true;
                    MyDebugPrintW(L"hToolbar is null, getlasterror:%u", GetLastError());
                    break;
                }

                if (hToolbar)
                {

                    // 获取工具栏中的控件数量
                    dwButtonCount = SendMessage(hToolbar, TB_BUTTONCOUNT, NULL, NULL);

                    GetWindowThreadProcessId(hToolbar, &dwProcessId);
                    hProcess = OpenProcess(PROCESS_ALL_ACCESS
                        | PROCESS_VM_OPERATION
                        | PROCESS_VM_READ
                        | PROCESS_VM_WRITE,
                        0,
                        dwProcessId);

                    lpAddress = VirtualAllocEx(hProcess, 0, sizeof(TBBUTTON), MEM_COMMIT, PAGE_READWRITE);
                    if (lpAddress == NULL)
                    {
                        failed = true;
                        MyDebugPrintW(L"lpAddress is NULL, Failed allocate memory, error code:%u", GetLastError());
                        break;
                    }
                    MyDebugPrintW(L"---------------------------dwButtonCount  is %d-----------------------", dwButtonCount);
                    for (INT i = 0; i < dwButtonCount; i++)
                    {
                        SendMessage(hToolbar, TB_GETBUTTON, i, (LPARAM)lpAddress);
                        ReadProcessMemory(hProcess, lpAddress, &tb, sizeof(tb), 0);

                        wchar_t szButtonText[256] = { 0 };

                        if (tb.iString != -1)
                        {
                            SendMessage(hToolbar, TB_GETBUTTONTEXT, tb.idCommand, (LPARAM)lpAddress);
                            ReadProcessMemory(hProcess, lpAddress, szButtonText, sizeof(szButtonText), 0);

                            MyDebugPrintW(L"szButtonText is %s,  i is %d", szButtonText, i);

                            if (!wcscmp(szButtonText, L"安全删除硬件并弹出媒体") || !wcscmp(szButtonText, L"Safely Remove Hardware and Eject Media"))
                            {
                                //MyDebugPrintW(L"szButtonText is %s,  i is %d", szButtonText, i);

                                SendMessage(hToolbar, TB_DELETEBUTTON, i, 0);

                                RECT rect;
                                GetWindowRect(hTrayWnd, &rect);
                                int width = rect.right - rect.left;
                                int height = rect.bottom - rect.top;

                                MyDebugPrintW(L"width is %d, height is %d", width, height);

                                SendMessage(hTrayWnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(width, height));
                            }
                        }
                        else
                        {
                            MyDebugPrintW(L"#Check that the read data is invalid!");
                        }
                    }

                    BOOL result = VirtualFreeEx(hProcess, lpAddress, 0, MEM_RELEASE);
                    if (result == FALSE) {
                        MyDebugPrintW(L"Failed to free memoery, error code:{}", GetLastError());
                    }
                }
                else
                {
                    failed = true;
                    MyDebugPrintW(L"Not find the toolbar!\n");
                }
            } while (false);

            if (hProcess)
            {
                CloseHandle(hProcess);
                hProcess = NULL;
            }

            Sleep(1000);
            //MyDebugPrintW(L"---------------------end-----------------------------------------------");
        }
    }
}
#endif