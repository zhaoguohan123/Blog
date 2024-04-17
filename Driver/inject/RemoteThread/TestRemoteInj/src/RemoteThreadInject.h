#ifndef _REMOTE_THREAD_INJECT_HEAD_H_
#define _REMOTE_THREAD_INJECT_HEAD_H_

#include "utils.h"

typedef struct PROC_INFO_STRUCT
{
    BOOL  bIsInjected;
    DWORD dwPID;
    std::wstring strProcName;
} PROC_INFO_STRUCT, *PPROC_INFO_STRUCT;


class RemoteThreadInject
{
private:
    std::wstring dll_path_;

    std::wstring proc_name_;

    std::shared_ptr<PROC_INFO_STRUCT> proc_info_;

    DWORD GetProcIdFromProcName(LPWSTR);

    BOOL CheckDllInProcess();

public:
    void Initialize();

    BOOL InjectDll();

    BOOL EjectDll();

    RemoteThreadInject(std::wstring , std::wstring );

    ~RemoteThreadInject();
};
#endif