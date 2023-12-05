#include "CommonHead.h"
#include "service_opt.h"
#include "list_kernel_obj.h"
//#include "DestructWithThreadNotEnd.h"
#include "Hide_Tray_icon.h"
#include "utils.h"
#include ".\GetProcInfo\GetProcInfo.h"

#pragma comment(lib, "Version.lib")
bool QueryValue(const std::string& ValueName, const std::string& szModuleName, std::string& RetStr);
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
        char processPath[MAX_PATH] = { 0 };
        DWORD bufferSize = MAX_PATH;

        // 获取进程可执行文件路径
        if (QueryFullProcessImageNameA(hProcess, 0, processPath, &bufferSize)) 
        {
            std::string RetStr;
            QueryValue("FileDescription", processPath, RetStr);
            std::cout<<"desc: "<<RetStr<<std::endl;
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

            if (wcslen(windowTitle) > 0){
                std::wcout << L"Process ID: " << processId  << std::endl;
                GetProcessDescription(processId);
                std::wcout << L"---------------------------------" << std::endl;
            }
        }
    }
}



bool QueryValue(const std::string& ValueName, const std::string& szModuleName, std::string& RetStr)
{
    bool bSuccess = FALSE;
    BYTE*  m_lpVersionData = NULL;
    DWORD   m_dwLangCharset = 0;
    CHAR *tmpstr = NULL;

    do
    {
        if (!ValueName.size() || !szModuleName.size())
        {
            LOGGER_ERROR("ValueName or szModuleName is empty");
            break;
        }
        DWORD dwHandle;
        // 判断系统能否检索到指定文件的版本信息
        DWORD dwDataSize = ::GetFileVersionInfoSizeA((LPCSTR)szModuleName.c_str(), &dwHandle);
        if (dwDataSize == 0)
        {
            LOGGER_ERROR("GetFileVersionInfoSizeA error: {}", GetLastError());
            break;
        }
 
        m_lpVersionData = new (std::nothrow) BYTE[dwDataSize];// 分配缓冲区
        if (NULL == m_lpVersionData)
        {
            LOGGER_ERROR("new error");
            break;
        }
 
        // 检索信息
        if (!::GetFileVersionInfoA((LPCSTR)szModuleName.c_str(), dwHandle, dwDataSize, (void*)m_lpVersionData))
        {
            LOGGER_ERROR("GetFileVersionInfoA error: {}", GetLastError());
            break;
        }
 
        UINT nQuerySize;
        DWORD* pTransTable;
        // 设置语言
        if (!::VerQueryValueA(m_lpVersionData, "\\VarFileInfo\\Translation", (void **)&pTransTable, &nQuerySize))
        {
            LOGGER_ERROR("VerQueryValueA error: {}", GetLastError());
            break;
        }
 
        m_dwLangCharset = MAKELONG(HIWORD(pTransTable[0]), LOWORD(pTransTable[0]));
        if (m_lpVersionData == NULL)
        {
            LOGGER_ERROR("m_lpVersionData is NULL");
            break;
        }
 
        tmpstr = new (std::nothrow) CHAR[256];// 分配缓冲区
        if (NULL == tmpstr)
        {
            LOGGER_ERROR("new error");
            break;
        }
        sprintf_s(tmpstr, 256, "\\StringFileInfo\\%08lx\\%s", m_dwLangCharset, ValueName.c_str());
        LPVOID lpData;
 
        // 调用此函数查询前需要先依次调用函数GetFileVersionInfoSize和GetFileVersionInfo
        if (::VerQueryValueA((void *)m_lpVersionData, tmpstr, &lpData, &nQuerySize))
        {
            RetStr = (char*)lpData;
        }else
        {
            LOGGER_ERROR("VerQueryValueA error: {}", GetLastError());
            break;
        }
            

        bSuccess = TRUE;
    } while (FALSE);

    // 销毁缓冲区
    if (m_lpVersionData)
    {
        delete[] m_lpVersionData;
        m_lpVersionData = NULL;
    }
    if (tmpstr)
    {
        delete[] tmpstr;
        tmpstr = NULL;
    }

    return bSuccess;
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



