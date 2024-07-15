#include "RemoteThreadInject.h"

RemoteThreadInject::RemoteThreadInject(std::wstring proc_name, std::wstring dll_name):
    dll_name_(dll_name),
    proc_info_(nullptr),
    proc_name_(proc_name)
{


}

RemoteThreadInject::~RemoteThreadInject()
{
}

VOID RemoteThreadInject::GetProcIdFromProcName(LPWSTR PName)
{
    HANDLE snap = INVALID_HANDLE_VALUE;
    PROCESSENTRY32 Process;
    Process.dwSize = sizeof(PROCESSENTRY32);

    do
    {
        snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if(snap == INVALID_HANDLE_VALUE)
        {
            LOGGER_ERROR("CreateToolhelp32Snapshot() failed!!! [{}]", GetLastError());
            break;
        }
        
        if (!Process32First(snap, &Process))
        {
            LOGGER_ERROR("Thread32First() failed!!! [{}]", GetLastError());
            break;
        }
        
        while (Process32Next(snap, &Process))
        {
            if (!_wcsnicmp(PName, Process.szExeFile, lstrlen(PName)) && Process.cntThreads >= 1)
            {
                proc_info_->pids.insert(Process.th32ProcessID);
            }
        }

    } while (FALSE);

    if (snap != INVALID_HANDLE_VALUE)
    {
        CloseHandle(snap);
    }
}

BOOL RemoteThreadInject::ProcessHasLoadDll(DWORD pid, std::wstring & dllname)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    while (INVALID_HANDLE_VALUE == hSnapshot)
    {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_BAD_LENGTH) 
        {
            hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            continue;
        }
        else 
        {
            LOGGER_ERROR("CreateToolhelp32Snapshot failed : {} currentProcessId : {} targetProcessId : {}",
                dwError, GetCurrentProcessId(), pid);
            return FALSE;
        }
    }
    MODULEENTRY32W mi{};
    mi.dwSize = sizeof(MODULEENTRY32W); //第一次使用必须初始化成员
    BOOL bRet = Module32FirstW(hSnapshot, &mi);
    while (bRet) 
    {
        // mi.szModule是短路径
        if (!_wcsnicmp(dllname.c_str(), mi.szModule, lstrlen(dllname.c_str()))) 
        {
            if (hSnapshot != NULL)
            {
                CloseHandle(hSnapshot);
                hSnapshot = NULL;
            }

            return TRUE;
        }
        mi.dwSize = sizeof(MODULEENTRY32W);
        bRet = Module32NextW(hSnapshot, &mi);
    }

    if (hSnapshot != NULL)
    {
        CloseHandle(hSnapshot);
        hSnapshot = NULL;
    }
    return FALSE;
}

BOOL RemoteThreadInject::EnableDebugPrivilege(BOOL bEnablePrivilege)
{
     HANDLE hProcess = NULL;
    TOKEN_PRIVILEGES tp{};
    LUID luid;
    hProcess = GetCurrentProcess();
    HANDLE hToken = NULL;
    
    OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &hToken);

    if (!LookupPrivilegeValueW(
        NULL,            // lookup privilege on local system
        SE_DEBUG_NAME,   // privilege to lookup 
        &luid))        // receives LUID of privilege
    {
        LOGGER_ERROR("LookupPrivilegeValue error: {}", GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        (PTOKEN_PRIVILEGES)NULL,
        (PDWORD)NULL))
    {
        LOGGER_ERROR("AdjustTokenPrivileges error: {}", GetLastError());
        CloseHandle(hToken);
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
        LOGGER_ERROR("The token does not have the specified privilege.");
        CloseHandle(hToken);
        return FALSE;
    }
    CloseHandle(hToken);
    return TRUE;
}

BOOL RemoteThreadInject::ZwCreateThreadExInjectDll(DWORD dwProcessId, std::wstring &pszDllFileName)
{
    BOOL                    bRet = FALSE;
    HANDLE                  hProcess = NULL;
    LPVOID                  pRemoteBuf = NULL;                                                      //目标进程开辟的内存的起始地址
    DWORD                   dwBufSize = (DWORD)(_tcslen(dll_path_.c_str()) + 1) * sizeof(TCHAR);    //开辟的内存的大小
    HMODULE                 hNtdll = NULL;
    HMODULE                 hKernel32 = NULL;
    FARPROC                 pFuncProcAddr = NULL;                                                     //loadLibreayW函数的起始地址
    HMODULE                 hMod = NULL;                                                            //kernel32.dll模块的句柄
    DWORD                   lpExitCode = 0;
    HANDLE                  hRemoteThread = NULL; 
    typedef_ZwCreateThreadEx ZwCreateThreadEx = NULL;

    do
    {
        if (!(hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId)))//打开目标进程，获得句柄
        {
            LOGGER_ERROR("OpenProcess({})!!! [{}]", dwProcessId, GetLastError());
            goto INJECTDLL_EXIT;
        }

        pRemoteBuf = VirtualAllocEx(hProcess, NULL, dwBufSize, MEM_COMMIT, PAGE_READWRITE);//在目标进程空间开辟一块内存
        if (pRemoteBuf == NULL)
        {
            LOGGER_ERROR("VirtualAllocEx() failed!!! [{}]", GetLastError());
            goto INJECTDLL_EXIT;
        }

        if (!WriteProcessMemory(hProcess, pRemoteBuf, (LPVOID)pszDllFileName.c_str(), dwBufSize, NULL))
        {
            LOGGER_ERROR("WriteProcessMemory() failed!!! [{}]", GetLastError());
            goto INJECTDLL_EXIT;
        }
        hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (NULL == hKernel32)
        {
            LOGGER_ERROR("GetModuleHandleW kernel32.dll failed! err:{}", GetLastError());
            goto INJECTDLL_EXIT;
        }

        pFuncProcAddr = GetProcAddress(hKernel32, "LoadLibraryW");
        if (NULL == pFuncProcAddr)
        {
            LOGGER_ERROR("GetProcAddress LoadLibraryW failed! err:{}",GetLastError());
            goto INJECTDLL_EXIT;
        }

        hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (NULL == hNtdll)
        {
            LOGGER_ERROR("GetModuleHandleW ntdll.dll failed! err:{}", GetLastError());
            goto INJECTDLL_EXIT;
        }

        ZwCreateThreadEx = (typedef_ZwCreateThreadEx)GetProcAddress(hNtdll, "ZwCreateThreadEx");
        if (NULL == ZwCreateThreadEx)
        {
            LOGGER_ERROR("get address of ZwCreateThreadEx failed! err:{}",GetLastError());
            goto INJECTDLL_EXIT;
        }

        ZwCreateThreadEx(&hRemoteThread, PROCESS_ALL_ACCESS, NULL, hProcess,  (LPTHREAD_START_ROUTINE)pFuncProcAddr, pRemoteBuf, 0, 0, 0, 0, NULL);
        if(NULL == hRemoteThread)
        {
            LOGGER_ERROR("ZwCreateThreadEx failed! err:{}", GetLastError());
            goto INJECTDLL_EXIT;
        }
        WaitForSingleObject(hRemoteThread, INFINITE);

        GetExitCodeThread(hRemoteThread, &lpExitCode);
        if (lpExitCode == 0)
        {
            LOGGER_ERROR("error inject failed!");
            goto INJECTDLL_EXIT;
        }

        bRet = TRUE;
    } while (FALSE);

INJECTDLL_EXIT:
    if (pRemoteBuf)
    {
        (void)VirtualFreeEx(hProcess, pRemoteBuf, 0, MEM_RELEASE);
        pRemoteBuf = NULL;
    }
    if (hProcess)
    {
        (void)CloseHandle(hProcess);
        hProcess = NULL;
    }
    if (hRemoteThread)
    {
        CloseHandle(hRemoteThread);
        hRemoteThread = NULL;
    }
    if (hNtdll)
    {
        FreeLibrary(hNtdll);
        hNtdll = NULL;
    }
    if (hKernel32)
    {
        FreeLibrary(hKernel32);
        hKernel32 = NULL;
    }
    
    return bRet;
}

BOOL RemoteThreadInject::Initialize()
{
    BOOL bRet = TRUE;
    proc_info_ = std::make_shared<PROC_INFO_STRUCT>();
    proc_info_->strProcName = proc_name_;

    TCHAR szPath[_MAX_PATH] = { 0 };
    TCHAR szDrive[_MAX_DRIVE] = { 0 };
    TCHAR szDir[_MAX_DIR] = { 0 };
    TCHAR szFname[_MAX_FNAME] = { 0 };
    TCHAR szExt[_MAX_EXT] = { 0 };
    TCHAR CurBinPath[MAX_PATH] = { 0 };

    if (0 == GetModuleFileNameW(NULL, szPath, sizeof(szPath) / sizeof(TCHAR)))
    {
        LOGGER_ERROR("GetModuleFileNameW failed. err:{}",GetLastError());
    }

    ZeroMemory(CurBinPath, sizeof(CurBinPath));
    _wsplitpath_s(szPath, szDrive, szDir, szFname, szExt);
    wsprintf(CurBinPath, L"%s%s", szDrive, szDir);

    if (FALSE == PathFileExistsW(CurBinPath))
    {
        LOGGER_ERROR("PathFileExistsW failed! err:{}", GetLastError());
        bRet = FALSE;
    }
    dll_path_ = std::wstring(CurBinPath) + dll_name_;
    LOGGER_INFO("dll path:{}", U2A(dll_path_.c_str()));

    return bRet;
}

VOID RemoteThreadInject::InjectDll()
{
    if (FALSE == EnableDebugPrivilege(TRUE)) 
    {
        goto INJECTDLL_EXIT;
    }

    GetProcIdFromProcName((LPWSTR)proc_name_.c_str());

    if (proc_info_->pids.size() == 0)
    {
        LOGGER_ERROR("GetProcIdFromProcName() failed!!! [{}]", GetLastError());
        goto INJECTDLL_EXIT;
    }

    for (auto dwPid: proc_info_->pids)
    {
        LOGGER_INFO("Inject pid({})", dwPid);
        if (ProcessHasLoadDll(dwPid, dll_name_) == TRUE)
        {
            LOGGER_ERROR("pid({}) has contained target dll!", dwPid);
            continue;
        }

        if (ZwCreateThreadExInjectDll(dwPid, dll_path_) == FALSE)
        {
            LOGGER_ERROR("Inject pid({}) failed! error:{}", dwPid, GetLastError());
        }
    }    

INJECTDLL_EXIT:
    EnableDebugPrivilege(FALSE);
}

VOID RemoteThreadInject::EjectDll()
{
    if (FALSE == EnableDebugPrivilege(TRUE)) 
    {
        goto INJECTDLL_EXIT;
    }

    GetProcIdFromProcName((LPWSTR)proc_name_.c_str());

    if (proc_info_->pids.size() == 0)
    {
        LOGGER_ERROR("GetProcIdFromProcName() failed!!! [{}]", GetLastError());
        goto INJECTDLL_EXIT;
    }

    for (auto dwPid: proc_info_->pids)
    {
        LOGGER_INFO("Eject pid({})", dwPid);
        if (ZwCreateThreadExEjectDll(dwPid, dll_name_) == FALSE)
        {
            LOGGER_ERROR("Eject pid({}) failed! error:{}", dwPid, GetLastError());
        }
    } 

INJECTDLL_EXIT:
    EnableDebugPrivilege(FALSE);
}

BOOL RemoteThreadInject::ZwCreateThreadExEjectDll(DWORD dwProcessId, std::wstring &pszDllFileName)
{
    BOOL                    bRet = FALSE;
    HANDLE                  hProcess = NULL;
    DWORD                   dwBufSize = (DWORD)(_tcslen(dll_path_.c_str()) + 1) * sizeof(TCHAR);    //开辟的内存的大小
    HMODULE                 hNtdll = NULL;
    HMODULE                 hKernel32 = NULL;
    FARPROC                 pFuncProcAddr = NULL;
    DWORD                   lpExitCode = 0;
    HANDLE                  hRemoteThread = NULL; 
    HANDLE                  hSnapshot = NULL;
    typedef_ZwCreateThreadEx ZwCreateThreadEx = NULL;

    do
    {
       hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, dwProcessId);
        while (INVALID_HANDLE_VALUE == hSnapshot)
        {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_BAD_LENGTH) 
            {
                hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, dwProcessId);
                continue;
            }
            else 
            {
                LOGGER_ERROR("CreateToolhelp32Snapshot failed : {} currentProcessId : {} targetProcessId : {}",
                    dwError, GetCurrentProcessId(), dwProcessId);
               goto INJECTDLL_EXIT;
            }
        }
        MODULEENTRY32W mi{};
        mi.dwSize = sizeof(MODULEENTRY32W); //第一次使用必须初始化成员

        BOOL isInjectedDll = FALSE;
        BOOL tmp = Module32FirstW(hSnapshot, &mi);
        if (tmp == FALSE)
        {
            LOGGER_ERROR("Module32FirstW failed : {}", GetLastError());
            goto INJECTDLL_EXIT;
        }
        
        while (tmp) 
        {
            // mi.szModule是短路径
            if (!_wcsnicmp(pszDllFileName.c_str(), mi.szModule, lstrlen(pszDllFileName.c_str()))) 
            {
                isInjectedDll = TRUE;
                break;
            }
            mi.dwSize = sizeof(MODULEENTRY32W);
            tmp = Module32NextW(hSnapshot, &mi);
        }

        if (isInjectedDll == FALSE)
        {
           LOGGER_ERROR("isInjectedDll == FALSE");
           goto INJECTDLL_EXIT;
        }

        if (!(hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId)))//打开目标进程，获得句柄
        {
            LOGGER_ERROR("OpenProcess({})!!! [{}]", dwProcessId, GetLastError());
            goto INJECTDLL_EXIT;
        }

        hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (NULL == hKernel32)
        {
            LOGGER_ERROR("GetModuleHandleW kernel32.dll failed! err:{}", GetLastError());
            goto INJECTDLL_EXIT;
        }        

        pFuncProcAddr = GetProcAddress(hKernel32, "FreeLibrary");
        if (NULL == pFuncProcAddr)
        {
            LOGGER_ERROR("GetProcAddress FreeLibrary failed! err:{}",GetLastError());
            goto INJECTDLL_EXIT;
        }

        hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (NULL == hNtdll)
        {
            LOGGER_ERROR("GetModuleHandleW ntdll.dll failed! err:{}", GetLastError());
            goto INJECTDLL_EXIT;
        }

        ZwCreateThreadEx = (typedef_ZwCreateThreadEx)GetProcAddress(hNtdll, "ZwCreateThreadEx");
        if (NULL == ZwCreateThreadEx)
        {
            LOGGER_ERROR("get address of ZwCreateThreadEx failed! err:{}",GetLastError());
            goto INJECTDLL_EXIT;
        }

        ZwCreateThreadEx(&hRemoteThread, PROCESS_ALL_ACCESS, NULL, hProcess,  (LPTHREAD_START_ROUTINE)pFuncProcAddr, mi.modBaseAddr, 0, 0, 0, 0, NULL);
        if(NULL == hRemoteThread)
        {
            LOGGER_ERROR("ZwCreateThreadEx failed! err:{}", GetLastError());
            goto INJECTDLL_EXIT;
        }
        WaitForSingleObject(hRemoteThread, INFINITE);

        GetExitCodeThread(hRemoteThread, &lpExitCode);
        if (lpExitCode == 0)
        {
            LOGGER_ERROR("error dwProcessId: {} Eject failed!", dwProcessId);
            goto INJECTDLL_EXIT;
        }

        bRet = TRUE;

    } while (FALSE);

INJECTDLL_EXIT:
    if (hSnapshot != NULL)
    {
        CloseHandle(hSnapshot);
        hSnapshot = NULL;
    }
    if (hProcess)
    {
        (void)CloseHandle(hProcess);
        hProcess = NULL;
    }
    if (hRemoteThread)
    {
        CloseHandle(hRemoteThread);
        hRemoteThread = NULL;
    }
    if (hNtdll)
    {
        FreeLibrary(hNtdll);
        hNtdll = NULL;
    }
    if (hKernel32)
    {
        FreeLibrary(hKernel32);
        hKernel32 = NULL;
    }
    
    return bRet;
}