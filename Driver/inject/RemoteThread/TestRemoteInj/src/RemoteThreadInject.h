#ifndef _REMOTE_THREAD_INJECT_HEAD_H_
#define _REMOTE_THREAD_INJECT_HEAD_H_

#include "utils.h"

typedef struct PROC_INFO_STRUCT
{
    BOOL  bIsInjected;
    std::set<DWORD> pids;
    std::wstring strProcName;
} PROC_INFO_STRUCT, *PPROC_INFO_STRUCT;


#ifdef _WIN64
    typedef DWORD(WINAPI* typedef_ZwCreateThreadEx)(
        PHANDLE ThreadHandle,
        ACCESS_MASK DesiredAccess,
        LPVOID ObjectAttributes,
        HANDLE ProcessHandle,
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        ULONG CreateThreadFlags,
        SIZE_T ZeroBits,
        SIZE_T StackSize,
        SIZE_T MaximumStackSize,
        LPVOID pUnkown
        );
#else
    typedef DWORD(WINAPI* typedef_ZwCreateThreadEx)(
        PHANDLE ThreadHandle,
        ACCESS_MASK DesiredAccess,
        LPVOID ObjectAttributes,
        HANDLE ProcessHandle,
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        BOOL CreateSuspended,
        DWORD dwStackSize,
        DWORD dw1,
        DWORD dw2,
        LPVOID pUnkown
        );
#endif 

class RemoteThreadInject
{
private:
    std::wstring dll_path_;

    std::wstring dll_name_;

    std::wstring proc_name_;

    std::shared_ptr<PROC_INFO_STRUCT> proc_info_;

    VOID GetProcIdFromProcName(LPWSTR);

    BOOL ProcessHasLoadDll(DWORD pid, std::wstring & dll);

    BOOL EnableDebugPrivilege(BOOL bEnablePrivilege);

    BOOL ZwCreateThreadExInjectDll(DWORD dwProcessId, std::wstring & pszDllFileName);

    BOOL ZwCreateThreadExEjectDll(DWORD dwProcessId, std::wstring & pszDllFileName);


public:
    BOOL Initialize();

    VOID InjectDll();

    VOID EjectDll();

    // 待注入的进程名，待注入的dll名
    RemoteThreadInject(std::wstring , std::wstring);

    ~RemoteThreadInject();
};
#endif