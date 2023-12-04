#include "GetProcInfo.h"

GetProcInfo::GetProcInfo()
{
}

GetProcInfo::~GetProcInfo()
{
}

std::string GetProcInfo::ConvertWideToMultiByte(const std::wstring& wideString) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wideString);
}

std::string GetProcInfo::GetProcPathByHandle(HANDLE hProcess)
{
    char proc_path[MAX_PATH + 1] = {0};
    if (!GetModuleFileNameExA(hProcess, NULL, proc_path, MAX_PATH))
    {
        LOGGER_ERROR("GetModuleFileNameExA error: {}", GetLastError());
    }
    return proc_path;
}


BOOL GetProcInfo::ExcludeSysProc()
{
    BOOL bRet = FALSE;

    do  // 除去系统进程
    {
        if (all_proc_info_.empty())
        {
            LOGGER_ERROR("all_proc_info_ is empty");
            break;
        }

        std::map<std::string, ROCESSINFOA>::iterator iter = all_proc_info_.begin();
        while (iter != all_proc_info_.end())
        {
            if (iter->second.proc_path.find("C:\\windows") != std::string::npos)
            {
                all_proc_info_.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

    } while (FALSE);
    bRet = TRUE;
    
    return TRUE;
}

BOOL GetProcInfo::GetAllProcInfo()
{
    BOOL bRet = FALSE;
    HANDLE hsnapshot = NULL;
    HANDLE hProcess = NULL;

    do
    {
        hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hsnapshot == INVALID_HANDLE_VALUE)
        {
            LOGGER_ERROR("CreateToolhelp32Snapshot Error!");
            break;
        }
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        int flag = Process32First(hsnapshot, &pe);
        while (flag != 0)
        {
            
            hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (hProcess !=  NULL)
            {
                ROCESSINFOA proc_info;
                proc_info.proc_id = std::to_string(pe.th32ProcessID);
                proc_info.proc_name = ConvertWideToMultiByte(pe.szExeFile);
                proc_info.proc_path = GetProcPathByHandle(hProcess);
                all_proc_info_.insert(std::make_pair(proc_info.proc_id, proc_info));
                LOGGER_INFO("pid: {}, name: {}, path: {}", proc_info.proc_id,  proc_info.proc_name, proc_info.proc_path);
            }else
            {
                LOGGER_INFO("OpenProcess pid {} error: {}", pe.th32ProcessID, GetLastError());
            }
            flag = Process32Next(hsnapshot, &pe);
        }

    } while (FALSE);
    bRet = TRUE;

    if (hsnapshot)
    {
        CloseHandle(hsnapshot);
        hsnapshot = NULL;
    }
    if (hProcess)
    {
        CloseHandle(hProcess);
        hProcess = NULL;
    }
    
    return bRet;
}