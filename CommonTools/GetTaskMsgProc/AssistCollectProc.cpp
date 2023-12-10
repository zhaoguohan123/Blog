#include "AssistCollectProc.h"

std::vector<HWND> g_top_level_wnds;

AssistCollectProc::AssistCollectProc():
    proc_id_vec_(),
    all_proc_info_(nullptr)
{
}

AssistCollectProc::~AssistCollectProc()
{
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId = 0;

    if (GetWindowThreadProcessId(hwnd, &processId) == 0)
    {
        LOGGER_ERROR("GetWindowThreadProcessId  error: {}", GetLastError());
    }
    if (IsWindowVisible(hwnd) && processId != 0)
    {
        g_top_level_wnds.push_back(hwnd);
    }
    
    return TRUE;
}

void AssistCollectProc::GetProcIdByTopWnd()
{
    EnumWindows(EnumWindowsProc, NULL);
    LOGGER_INFO("top_level_wnds_ size: {}", g_top_level_wnds.size());

    for (HWND hwnd : g_top_level_wnds) 
    {
        DWORD processId = 0;

        (void)GetWindowThreadProcessId(hwnd, &processId);

        if (processId != 0) {
            WCHAR windowTitle[MAX_PATH] = { 0 };
            (void)GetWindowTextW(hwnd, windowTitle, MAX_PATH);
            if (wcslen(windowTitle) > 0)
            {
                proc_id_vec_.push_back(processId);
            }
        }
    }
}

void AssistCollectProc::GetProcInfoByPid(DWORD processId, 
                                        std::string & desc_str, 
                                        std::string & version_str,
                                        std::chrono::milliseconds & duration)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess != NULL) 
    {
        char processPath[MAX_PATH] = { 0 };
        DWORD bufferSize = MAX_PATH;

        // 获取进程可执行文件路径
        if (QueryFullProcessImageNameA(hProcess, 0, processPath, &bufferSize)) 
        {
            if(!QueryValue("FileDescription", processPath, desc_str))
            {
                LOGGER_ERROR("QueryValue processId:{} FileDescription!",processId);
            }
            if(!QueryValue("ProductVersion", processPath, version_str))
            {
                LOGGER_ERROR("QueryValue processId:{} ProductVersion!",processId);
            }
            if(!GetProcRunTimeByHandle(hProcess, duration))
            {
                LOGGER_ERROR("GetProcRunTimeByHandle ");
            }

        }
        else
        {
            LOGGER_ERROR("QueryFullProcessImageNameW error: {}", GetLastError());
        }

        if (hProcess)
        {
            CloseHandle(hProcess);
            hProcess = NULL;
        }
    }
    else
    {
        LOGGER_ERROR("OpenProcess error: {}", GetLastError());
    }
}

BOOL AssistCollectProc::GetProcRunTimeByHandle(HANDLE hProcess, std::chrono::milliseconds & duration)
{
    BOOL bRet = FALSE;
    do 
    { 
        if (hProcess != NULL) 
        {
            FILETIME createTime, exitTime, kernelTime, userTime;

            if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
                FILETIME now;
                (void)GetSystemTimeAsFileTime(&now);

                ULONGLONG createTime64 = ((ULONGLONG)createTime.dwHighDateTime << 32) + createTime.dwLowDateTime;
                ULONGLONG now64 = ((ULONGLONG)now.dwHighDateTime << 32) + now.dwLowDateTime;
                ULONGLONG elapsedTime64 = now64 - createTime64;

                ULONGLONG milliseconds = elapsedTime64 / 10000;

                std::chrono::milliseconds duration_tmp(milliseconds);
                duration = duration_tmp;
                // std::chrono::hours hours = std::chrono::duration_cast<std::chrono::hours>(duration);
                // duration -= std::chrono::duration_cast<std::chrono::milliseconds>(hours);
                // std::chrono::minutes minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
                // duration -= std::chrono::duration_cast<std::chrono::milliseconds>(minutes);
                // std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);

                // std::stringstream ss;
                // if (hours.count() > 0) {
                //     ss << hours.count() << "h-";
                // }
                // if (minutes.count() >= 0) {
                //     ss << minutes.count() << "m-";
                // }
                // if (seconds.count() >= 0 || (hours.count() == 0 && minutes.count() == 0)) {
                //     ss << seconds.count() << "s";
                // }
                // RetStr = ss.str();
            } 
            else 
            {
                LOGGER_ERROR("Failed to get process times. Error code: {}", GetLastError());
                break;
            }
        }
        bRet = TRUE;
    }
    while(FALSE);

    return bRet;
}

BOOL AssistCollectProc::QueryValue(const std::string &ValueName, 
                                    const std::string &szModuleName, 
                                    std::string &RetStr)
{
    BOOL bSuccess = FALSE;
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
            LOGGER_ERROR("new error m_lpVersionData is NULL");
            break;
        }
 
        // 检索信息
        if (!::GetFileVersionInfoA((LPCSTR)szModuleName.c_str(), dwHandle, dwDataSize, (void*)m_lpVersionData))
        {
            LOGGER_ERROR("GetFileVersionInfoA szModuleName: {} error: {}", szModuleName.c_str(), GetLastError());
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
            LOGGER_ERROR("VerQueryValueA szModuleName:{} error: {}", szModuleName.c_str(), GetLastError());
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

std::shared_ptr<std::map<std::string, ROCESSINFOA>> AssistCollectProc::GetProcInfo()
{
    std::shared_ptr<std::map<std::string, ROCESSINFOA>> proc_info_vec = std::make_shared<std::map<std::string, ROCESSINFOA>>();
    (void)GetProcIdByTopWnd();

    std::string  proc_desc;
    std::string  proc_version;
    std::chrono::milliseconds duration;

    for (auto pid:proc_id_vec_)
    {
        (void)GetProcInfoByPid(pid, proc_desc, proc_version, duration);

        if (proc_desc.size() > 0)
        {
            ROCESSINFOA proc_info;
            proc_info.proc_running_time = duration;
            proc_info.proc_version = proc_version;
            proc_info_vec->insert(std::make_pair(proc_desc, proc_info));
        }
    }
    return proc_info_vec;
}
