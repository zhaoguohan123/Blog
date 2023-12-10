#ifndef _GET_TASK_MSG_PROC_INFO_
#define _GET_TASK_MSG_PROC_INFO_

#include "../../Common/CommonHead.h"
#include <iostream>
#include <string>
#include <vector>
#include <psapi.h>
#include <Windows.h>
#include <map>
#include <codecvt>
#include <locale>

#pragma comment(lib, "Version.lib")

typedef struct LANGANDCODEPAGE {
  WORD wLanguage;
  WORD wCodePage;
} *lpTranslate;

typedef struct PROCESSINFO
{
    std::string proc_version;
    std::chrono::milliseconds proc_running_time;
}ROCESSINFOA, *PROCESSINFOA;


class AssistCollectProc
{
public:
    AssistCollectProc();
    ~AssistCollectProc();

    //枚举窗口获取进程PID
    void GetProcIdByTopWnd();
    
    //通过PID获取进程描述信息，版本信息，运行时间
    void GetProcInfoByPid(DWORD processId,
                          std::string & desc_str, 
                          std::string & version_str,
                          std::chrono::milliseconds & duration);

    //通过进程句柄获取进程运行时间
    BOOL GetProcRunTimeByHandle(HANDLE hProcess, 
                                std::chrono::milliseconds & duration);

    //根据进程二进制所在路径，获取指定属性(版本号，描述信息)
    BOOL QueryValue(const std::string& ValueName, 
                    const std::string& szModuleName, 
                    std::string& RetStr);

    //查询任务管理器，进程选项卡中，应用的信息
    std::shared_ptr<std::map<std::string, ROCESSINFOA>> GetProcInfo();

    //保存每次枚举窗口获取的PID
    std::vector<DWORD> proc_id_vec_;

    //保存15分钟类采集到的进程数据
    std::shared_ptr<std::map<std::string, ROCESSINFOA>> all_proc_info_;

    //将进程运行的时间转换成格式 39:34:12 (39小时34分钟12秒)
    BOOL TransTimeFormat();

    // 将收集转换成json格式字符串用于上报
    std::string TransJsonToString(nlohmann::json & json_object);

    // 运行工作线程; 
    void Run();
    
    // 停止工作线程;
    void Stop();

    //1. 每15分钟上报一次收集到的软件信息  2. 每分钟收集一次软件信息
    void WorkThread();

    // 把json数据上传给平台
    BOOL SendProcInfoToPlat();
};

#endif