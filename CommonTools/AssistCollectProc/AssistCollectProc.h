#ifndef _ASSIST_COLLECT_PROC_PROC_INFO_
#define _ASSIST_COLLECT_PROC_PROC_INFO_

#include "../../Common/CommonHead.h"
#include <iostream>
#include <string>
#include <vector>
#include <psapi.h>
#include <Windows.h>
#include <map>
#include <codecvt>
#include <locale>
#include "json.hpp"

#pragma comment(lib, "Version.lib")

typedef struct LANGANDCODEPAGE {
  WORD wLanguage;
  WORD wCodePage;
} *lpTranslate;

typedef struct PROCESSINFO
{
    std::string proc_version;
    int proc_run_time_miniutes;
}ROCESSINFOA, *PROCESSINFOA;

class AssistCollectProc
{
public:
    AssistCollectProc();
    ~AssistCollectProc();

    // 运行工作线程; 
    void Run();
    
    // 停止工作线程;
    void Stop();

    // 初始化
    BOOL Initialize();

    //1. 每15分钟上报一次收集到的软件信息  2. 每分钟收集一次软件信息
    void WorkThread();

    //枚举窗口获取进程PID
    void GetProcIdByTopWnd();
    
    //通过PID获取进程描述信息，版本信息
    void GetProcInfoByPid(DWORD processId,
                          std::string & desc_str, 
                          std::string & version_str);

    //通过进程句柄获取进程运行时间
    // BOOL GetProcRunTimeByHandle(HANDLE hProcess, 
    //                             int & duration);

    //根据进程二进制所在路径，获取指定属性(版本号，描述信息)
    BOOL QueryValue(const std::wstring& ValueName, 
                    const std::wstring& szModuleName, 
                    std::string& RetStr);

    //查询任务管理器，进程选项卡中，应用的信息
    std::shared_ptr<std::map<std::string, ROCESSINFOA>> GetProcInfo();

    // // 把json数据上传给平台
    // BOOL SendProcInfoToPlat();

    // 将每分钟采集到的数据，保存到15分钟内采集到的数据中
    void SaveProcInfo(std::shared_ptr<std::map<std::string, ROCESSINFOA>> & proc_info_vec);

    // 将收集到的数据，转换为json字符串格式
    std::string ConvertProcInfoToJson();

    // 获取虚拟机uuid
    std::string GetVmUuid();
    
    //保存每次枚举窗口获取的PID
    std::vector<DWORD> proc_id_vec_;

    //保存15分钟类采集到的进程数据
    std::shared_ptr<std::map<std::string, ROCESSINFOA>> all_proc_info_;

    // 进程指针
    std::shared_ptr<std::thread> proc_work_thread_;

    HANDLE  exit_event_;

    std::atomic_bool initialized_;
};

#endif