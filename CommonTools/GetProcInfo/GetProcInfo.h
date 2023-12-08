#ifndef _GET_PROC_INFO_
#define _GET_PROC_INFO_

#include "../../Common/CommonHead.h"
#include <iostream>
#include <string>
#include <vector>
#include <psapi.h>
#include <Windows.h>
#include <map>
#include <codecvt>
#include <locale>

namespace GetProcInfoSpace
{

    typedef struct PROCESSINFO
    {
        std::string proc_id;
        std::string proc_path;
        std::string proc_name;

    }ROCESSINFOA, *PROCESSINFOA;


    class   GetProcInfo
    {
    public:
        GetProcInfo();
        ~GetProcInfo();

        // 根据PID获取进程相关信息
        BOOL GetAllProcInfo();

        // 根据进程句柄获取exe所在路径
        std::string GetProcPathByHandle(HANDLE hProcess);

        // 排除系统进程
        BOOL ExcludeSysProc();

        // 所有进程的信息
        std::map<std::string , ROCESSINFOA> all_proc_info_ ;

        std::string GetProcInfo::ConvertWideToMultiByte(const std::wstring& wideString);
    };
}

#endif