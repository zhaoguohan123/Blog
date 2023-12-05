#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"
//#include "DestructWithThreadNotEnd.h"
#include "Hide_Tray_icon.h"
#include "utils.h"
#include ".\GetProcInfo\GetProcInfo.h"

#pragma comment(lib, "Version.lib")

struct LANGANDCODEPAGE {
  WORD wLanguage;
  WORD wCodePage;
} *lpTranslate;


#include <Psapi.h>
std::wstring GetProcessDescription(DWORD processId) {
    std::wstring description;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess != NULL) 
    {
        WCHAR processPath[MAX_PATH] = { 0 };
        DWORD bufferSize = MAX_PATH;

        // 获取进程可执行文件路径
        if (QueryFullProcessImageNameW(hProcess, 0, processPath, &bufferSize)) 
        {
            // 获取文件版本信息
            DWORD handle;
            DWORD fileInfoSize = GetFileVersionInfoSizeW(processPath, &handle);
            if (fileInfoSize > 0) 
            {
                std::vector<BYTE> fileInfoBuffer(fileInfoSize);
                if (GetFileVersionInfoW(processPath, handle, fileInfoSize, &fileInfoBuffer[0])) {
                    LPVOID fileDescription;
                    UINT size; //
                    if (VerQueryValueW(&fileInfoBuffer[0], L"\\StringFileInfo\\080404b0\\FileDescription", (LPVOID*)&fileDescription, &size)) 
                    {
                        description = reinterpret_cast<WCHAR*>(fileDescription);
                    }else
                    {
                        LOGGER_ERROR("VerQueryValueW error: {}", GetLastError());
                        if (VerQueryValueW(&fileInfoBuffer[0], L"\\StringFileInfo\\040904b0\\FileDescription", (LPVOID*)&fileDescription, &size)) 
                        {
                            description = reinterpret_cast<WCHAR*>(fileDescription);
                        }else
                        {
                            LOGGER_ERROR("VerQueryValueW error: {}", GetLastError());
                        }
                    }
                }else
                {
                    LOGGER_ERROR("GetFileVersionInfoW error: {}", GetLastError());
                }
            }else
            {
                LOGGER_ERROR("GetFileVersionInfoSizeW error: {}", GetLastError());
            }
        }else
        {
            LOGGER_ERROR("QueryFullProcessImageNameW error: {}", GetLastError());
        }
        CloseHandle(hProcess);
    }else
    {
        LOGGER_ERROR("OpenProcess error: {}", GetLastError());
    }

    return description;
}

std::vector<HWND> topLevelWindows;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (IsWindowVisible(hwnd) && processId != 0) 
    {
        topLevelWindows.push_back(hwnd);
    }
    
    return TRUE;
}

void ListProcessesFromWindows() {
    EnumWindows(EnumWindowsProc, 0);

    // Get process ID for each top level window
    for (HWND hwnd : topLevelWindows) 
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);

        if (processId != 0) {
            WCHAR windowTitle[MAX_PATH] = { 0 };
            GetWindowTextW(hwnd, windowTitle, MAX_PATH);

            if (wcslen(windowTitle) > 0) {
                //std::wcout << L"Window Title: " << windowTitle << std::endl;
                std::wcout << L"Process ID: " << processId  << L"   " <<  L"desc:" << GetProcessDescription(processId) << std::endl;
                std::wcout << L"---------------------------------" << std::endl;
            }
        }
    }
}


// 创建一个服务，并在服务中输出传入的参数
int main(int argc, TCHAR* argv[])
{
    init_logger("logs.log");

    //1 . 创建服务
    //serv_opt::run_serv(argc, argv);
    
    // 2.遍历所有的内核obj
    //MyWinobj::main();


    //3. 类析构时，类中的线程未退出，且在使用类中对象
    //DestructWithThreadNotEnd::main();
    
    //HIDE_TRAY_ICON::DeleteTrayIcon();
    // GetProcInfo a;
    // a.GetAllProcInfo();
    // a.ExcludeSysProc();

    ListProcessesFromWindows();
    getchar();
    return 0;
}



