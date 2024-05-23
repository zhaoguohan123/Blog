#include "RemoteThreadInject.h"

RemoteThreadInject::RemoteThreadInject(std::wstring proc_name, std::wstring dll_path):
    dll_path_(dll_path),
    proc_info_(nullptr),
    proc_name_(proc_name)
{


}

RemoteThreadInject::~RemoteThreadInject()
{
}

DWORD RemoteThreadInject::GetProcIdFromProcName(LPWSTR PName)
{
    DWORD bRet = 0;
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
            if (_wcsnicmp(PName, Process.szExeFile, lstrlen(PName)) == 0)
            {
                bRet = Process.th32ProcessID;
                LOGGER_INFO("proc id is {}",bRet);
                break;
            }
        }

    } while (FALSE);

    if (snap != INVALID_HANDLE_VALUE)
    {
        CloseHandle(snap);
    }

    return bRet;
}

void RemoteThreadInject::Initialize()
{
    proc_info_ = std::make_shared<PROC_INFO_STRUCT>();
    proc_info_->dwPID = GetProcIdFromProcName((LPWSTR)proc_name_.c_str());
    proc_info_->strProcName = proc_name_;
}

BOOL RemoteThreadInject::InjectDll()
{
    HANDLE                  hProcess = NULL;                                                        //保存目标进程的句柄
    LPVOID                  pRemoteBuf = NULL;                                                      //目标进程开辟的内存的起始地址
    DWORD                   dwBufSize = (DWORD)(_tcslen(dll_path_.c_str()) + 1) * sizeof(TCHAR);    //开辟的内存的大小
    LPTHREAD_START_ROUTINE  pThreadProc = NULL;                                                     //loadLibreayW函数的起始地址
    HMODULE                 hMod = NULL;                                                            //kernel32.dll模块的句柄
    BOOL                    bRet = FALSE;
    if (!(hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc_info_->dwPID)))//打开目标进程，获得句柄
    {
        LOGGER_ERROR("OpenProcess({})!!! [{}]", proc_info_->dwPID, GetLastError());
        goto INJECTDLL_EXIT;
    }
    pRemoteBuf = VirtualAllocEx(hProcess, NULL, dwBufSize, MEM_COMMIT, PAGE_READWRITE);//在目标进程空间开辟一块内存
    if (pRemoteBuf == NULL)
    {
        LOGGER_ERROR("VirtualAllocEx() failed!!! [{}]", GetLastError());
        goto INJECTDLL_EXIT;
    }
    if (!WriteProcessMemory(hProcess, pRemoteBuf,
        (LPVOID)(dll_path_.c_str()), dwBufSize, NULL))//向开辟的内存复制dll的路径
    {
        LOGGER_ERROR("WriteProcessMemory() failed!!! [{}]", GetLastError());
        goto INJECTDLL_EXIT;
    }
    hMod = GetModuleHandle(L"kernel32.dll");//获得本进程kernel32.dll的模块句柄
    if (hMod == NULL)
    {
        LOGGER_ERROR("InjectDll() : GetModuleHandle(\"kernel32.dll\") failed!!! [{}]", GetLastError());
        goto INJECTDLL_EXIT;
    }
    pThreadProc = (LPTHREAD_START_ROUTINE)GetProcAddress(hMod, "LoadLibraryW");//获得LoadLibraryW函数的起始地址
    if (pThreadProc == NULL)
    {
        LOGGER_ERROR("InjectDll() : GetProcAddress(\"LoadLibraryW\") failed!!! [{}]", GetLastError());
        goto INJECTDLL_EXIT;
    }
    HANDLE hRemotehandle = CreateRemoteThread(hProcess, NULL, 0, pThreadProc, pRemoteBuf, 0, NULL);
    if (!hRemotehandle)
    {
        LOGGER_ERROR("InjectDll: MyCreateRemoteThread{} failed!!!", GetLastError());
        goto INJECTDLL_EXIT;
    }
    WaitForSingleObject(hRemotehandle, INFINITE);

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
    if (hRemotehandle)
    {
        CloseHandle(hRemotehandle);
        hRemotehandle = NULL;
    }

    return bRet;
}

BOOL RemoteThreadInject::CheckDllInProcess()
{
    // exe正在加载其他模块时，可能会报错
    BOOL                    bMore = FALSE;
    HANDLE                  hSnapshot = INVALID_HANDLE_VALUE;
    MODULEENTRY32           me = { sizeof(me), };
    

    if (INVALID_HANDLE_VALUE ==(hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, proc_info_->dwPID)))//获得进程的快照
    {
        LOGGER_ERROR("CheckDllInProcess() : CreateToolhelp32Snapshot({}) failed!!! [{}]", proc_info_->dwPID, GetLastError());
        return FALSE;
    }

    bMore = Module32First(hSnapshot, &me);//遍历进程内得的所有模块
    for (; bMore; bMore = Module32Next(hSnapshot, &me))
    {
        if (!_tcsicmp(me.szModule, dll_path_.c_str()) || !_tcsicmp(me.szExePath,  dll_path_.c_str()))//模块名或含路径的名相符
        {
            (void)CloseHandle(hSnapshot);
            return TRUE;
        }
    }
    (void)CloseHandle(hSnapshot);
    return FALSE;
}

BOOL RemoteThreadInject::EjectDll()
{
    DWORD                   dwPID = proc_info_->dwPID;
    LPWSTR                  szDllPath = (LPWSTR)dll_path_.c_str();

    BOOL                    bMore = FALSE, bFound = FALSE, bRet = FALSE;
    HANDLE                  hSnapshot = INVALID_HANDLE_VALUE;
    HANDLE                  hProcess = NULL;
    MODULEENTRY32           me = { sizeof(me), };
    LPTHREAD_START_ROUTINE  pThreadProc = NULL;
    HMODULE                 hMod = NULL;
    TCHAR                   szProcName[MAX_PATH] = { 0, };
    
    if (INVALID_HANDLE_VALUE == (hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID)))
    {
        LOGGER_ERROR("EjectDll() : CreateToolhelp32Snapshot({}) failed!!! [{}]",  dwPID, GetLastError());
        goto EJECTDLL_EXIT;
    }
    bMore = Module32First(hSnapshot, &me);
    for (; bMore; bMore = Module32Next(hSnapshot, &me))//查找模块句柄
    {
        if (!_tcsicmp(me.szModule, szDllPath) || !_tcsicmp(me.szExePath, szDllPath))
        {
            bFound = TRUE;
            break;
        }
    }
    if (!bFound)
    {
        LOGGER_ERROR("EjectDll() : There is not {} module in process({}) memory!!!", U2A(szDllPath), dwPID);
        goto EJECTDLL_EXIT;
    }
    if (!(hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID)))
    {
        LOGGER_ERROR("EjectDll() : OpenProcess({}) failed!!! [{}]", dwPID, GetLastError());
        goto EJECTDLL_EXIT;
    }
    hMod = GetModuleHandle(L"kernel32.dll");
    if (hMod == NULL)
    {
        LOGGER_ERROR("EjectDll() : GetModuleHandle(\"kernel32.dll\") failed!!! [{}]", GetLastError());
        goto EJECTDLL_EXIT;
    }
    pThreadProc = (LPTHREAD_START_ROUTINE)GetProcAddress(hMod, "FreeLibrary");
    if (pThreadProc == NULL)
    {
        LOGGER_ERROR("EjectDll() : GetProcAddress(\"FreeLibrary\") failed!!! [{}]", GetLastError());
        goto EJECTDLL_EXIT;
    }
    if (!CreateRemoteThread(hProcess, NULL, 0, pThreadProc, me.modBaseAddr, 0, NULL))
    {
        LOGGER_ERROR("EjectDll() : MyCreateRemoteThread() failed!!!");
        goto EJECTDLL_EXIT;
    }
    bRet = TRUE;
EJECTDLL_EXIT:
    if (hProcess)
    {
        (void)CloseHandle(hProcess);
        hProcess = NULL;
    }
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        (void)CloseHandle(hSnapshot);
        hSnapshot = INVALID_HANDLE_VALUE;
    }

    return bRet;
}