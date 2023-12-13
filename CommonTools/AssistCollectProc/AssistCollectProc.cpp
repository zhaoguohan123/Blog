#include "AssistCollectProc.h"

std::vector<HWND> g_top_level_wnds;

AssistCollectProc::AssistCollectProc():
    proc_id_vec_(),
    proc_work_thread_(nullptr),
    exit_event_(NULL),
    initialized_(false)
{
    all_proc_info_ = std::make_shared<std::map<std::string, ROCESSINFOA>>();
}

AssistCollectProc::~AssistCollectProc()
{
    (void)Stop();

    if (exit_event_)
    {
        CloseHandle(exit_event_);
        exit_event_ = NULL;
    }
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
                                        std::string & version_str)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess != NULL) 
    {
        char processPath[MAX_PATH] = { 0 };
        DWORD bufferSize = MAX_PATH;

        // 获取进程可执行文件路径
        if (QueryFullProcessImageNameA(hProcess, 0, processPath, &bufferSize)) 
        {
            (void)QueryValue("FileDescription", processPath, desc_str);
            (void)QueryValue("ProductVersion", processPath, version_str);

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
}

BOOL AssistCollectProc::QueryValue(const std::string &Val, 
                                    const std::string &szModule, 
                                    std::string &RetStr)
{
    std::wstring ValueName = UTF8ToUTF16(Val);
    std::wstring szModuleName = UTF8ToUTF16(szModule);
    std::wstring RetWStr;

    BOOL bSuccess = FALSE;
    BYTE*  m_lpVersionData = NULL;
    DWORD   m_dwLangCharset = 0;
    WCHAR *tmpstr = NULL;

    do
    {
        if (ValueName.empty() || szModuleName.empty())
        {
            LOGGER_ERROR("ValueName or szModuleName is empty");
            break;
        }
        DWORD dwHandle;
        // 判断系统能否检索到指定文件的版本信息
        DWORD dwDataSize = ::GetFileVersionInfoSizeW((LPCWSTR)szModuleName.c_str(), &dwHandle);
        if (dwDataSize == 0)
        {
            break;
        }

        m_lpVersionData = new (std::nothrow) BYTE[dwDataSize];// 分配缓冲区
        if (NULL == m_lpVersionData)
        {
            LOGGER_ERROR("new error m_lpVersionData is NULL");
            break;
        }
 
        // 检索信息
        if (!::GetFileVersionInfoW((LPCWSTR)szModuleName.c_str(), dwHandle, dwDataSize, (void*)m_lpVersionData))
        {
            break;
        }
 
        UINT nQuerySize;
        DWORD* pTransTable;
        // 设置语言
        if (!::VerQueryValueW(m_lpVersionData, L"\\VarFileInfo\\Translation", (void **)&pTransTable, &nQuerySize))
        {
            break;
        }
 
        m_dwLangCharset = MAKELONG(HIWORD(pTransTable[0]), LOWORD(pTransTable[0]));
        if (m_lpVersionData == NULL)
        {
            LOGGER_ERROR("m_lpVersionData is NULL");
            break;
        }
 
        tmpstr = new (std::nothrow) WCHAR[256];// 分配缓冲区
        if (NULL == tmpstr)
        {
            LOGGER_ERROR("new error");
            break;
        }
        swprintf_s(tmpstr, 256, L"\\StringFileInfo\\%08lx\\%s", m_dwLangCharset, ValueName.c_str());
        LPVOID lpData;
       
        // 调用此函数查询前需要先依次调用函数GetFileVersionInfoSize和GetFileVersionInfo
        if (::VerQueryValueW((void *)m_lpVersionData, tmpstr, &lpData, &nQuerySize))
        {
            RetWStr = (WCHAR*)lpData;
        }
        RetStr = UTF16ToUTF8(RetWStr);
        
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

    for (auto pid:proc_id_vec_)
    {
        (void)GetProcInfoByPid(pid, proc_desc, proc_version);

        if (proc_desc.size() > 0)
        {
            ROCESSINFOA proc_info;
            proc_info.proc_run_time_miniutes = 0;
            proc_info.proc_version = proc_version;
            proc_info_vec->insert(std::make_pair(proc_desc, proc_info));
        }
    }
    return proc_info_vec;
}

void AssistCollectProc::Run()
{
    if (!proc_work_thread_)
    {
        proc_work_thread_ = std::make_shared<std::thread>(&AssistCollectProc::WorkThread, this);
    }
    else
    {
        LOGGER_ERROR("proc_work_thread_ is running!");
    }
}

void AssistCollectProc::Stop()
{
    SetEvent(exit_event_);

    if (proc_work_thread_ && proc_work_thread_->joinable())
    {
        proc_work_thread_->join();
        proc_work_thread_ .reset();
    }
}

void AssistCollectProc::WorkThread()
{
    ResetEvent(exit_event_);
    while (TRUE)
    {
        DWORD count = 0;

        while (count++ < 15)  // 计时15分钟
        {
            DWORD dwRet = WaitForSingleObject(exit_event_, 1 * 1000);  // 计时1分钟
            if (WAIT_OBJECT_0 == dwRet)
            {
                return; // 退出线程
            }
            else if (WAIT_TIMEOUT == dwRet) // 不在all_proc_info_中，则新增该应用的记录；在all_proc_info_中，则运行时间加一分钟
            {
                // 采集应用信息
                std::shared_ptr<std::map<std::string, ROCESSINFOA>> proc_info_vec = GetProcInfo();
                (void)SaveProcInfo(proc_info_vec);
            }
            else
            {
                LOGGER_ERROR("WaitForSingleObject error: {}", GetLastError());
                return; 
            }
        }

        // 将收集数据转换成json格式字符串
        std::string app_info_json = ConvertProcInfoToJson();
        if (app_info_json.size() <= 0)
        {
            LOGGER_ERROR("ConvertProcInfoToJson error");
            continue;
        }
        LOGGER_INFO("app_info_json: {}", app_info_json);
        // 上传数据给平台
    }
}

BOOL AssistCollectProc::Initialize()
{
    if  (initialized_)
    {
        return TRUE;
    }
    BOOL bRet = FALSE;
    do
    {
        exit_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (NULL == exit_event_)
        {
            LOGGER_ERROR("CreateEvent error: {}", GetLastError());
            break;
        }
        initialized_ = true;
        bRet = TRUE;
    } while (FALSE);

    return bRet;
}


void AssistCollectProc::SaveProcInfo(std::shared_ptr<std::map<std::string, ROCESSINFOA>> & proc_info_vec)
{
    // 将每分钟采集到的数据，保存到15分钟内采集到的数据中
    if (proc_info_vec->size() > 0)
    {
        if (all_proc_info_ == nullptr)  // 第一次采集数据
        {
            all_proc_info_ = proc_info_vec;
        }
        else
        {
            for (auto it = proc_info_vec->begin(); it != proc_info_vec->end(); ++it) // 遍历每分钟采集到的数据
            {
                if (all_proc_info_->find(it->first) != all_proc_info_->end())
                {
                    // 应用在all_proc_info_中，则运行时间加一分钟
                    all_proc_info_->at(it->first).proc_run_time_miniutes += 1;
                }
                else
                {
                    // 应用不在all_proc_info_中，则新增该应用的记录
                    all_proc_info_->insert(std::make_pair(it->first, it->second));
                }
            }
        }
    }
}

std::string AssistCollectProc::ConvertProcInfoToJson()
{
    nlohmann::json send_proc_data_json;
    send_proc_data_json["VMuuid"] = GetVmUuid();
    
    nlohmann::json appDataArray = nlohmann::json::array();

    for (auto it = all_proc_info_->begin(); it != all_proc_info_->end(); it++)
    {
        nlohmann::json appData;
        appData["AppName"] = it->first;
        appData["Version"] = it->second.proc_version;
        appData["RunningTime"] = it->second.proc_run_time_miniutes;

        appDataArray.push_back(appData);
    }
    
    send_proc_data_json["AppData"] = appDataArray;

    return send_proc_data_json.dump();
}

std::string AssistCollectProc::GetVmUuid()
{
    std::string retStr;
    HKEY hKey = NULL;
    DWORD dwType = REG_SZ;
    DWORD dwSize = 0;

    do {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\TroilaKly\\FeatureAssist", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExA(hKey, "vmUuid", NULL, &dwType, NULL, &dwSize) == ERROR_SUCCESS) {
                char* buffer = new char[dwSize];
                if (buffer == NULL) {
                    LOGGER_ERROR("new buffer failed!");
                    break;
                }

                if (RegQueryValueExA(hKey, "vmUuid", NULL, NULL, reinterpret_cast<LPBYTE>(buffer), &dwSize) == ERROR_SUCCESS) {
                    retStr = buffer;
                }
                else {
                    LOGGER_ERROR("RegQueryValueExA failed! errcode : {}", GetLastError());
                }
                if (buffer) {
                    delete[] buffer;
                    buffer = NULL;
                }
            }
            else {
                LOGGER_ERROR("RegQueryValueExA sting size failed! errcode : {}", GetLastError());
            }
        }
        else {
            LOGGER_ERROR("RegOpenKeyExA key failed! errcode : {}", GetLastError());
        }
    } while (FALSE);
    
    if (hKey)
    {
        RegCloseKey(hKey);
        hKey = NULL;
    }

    return retStr;
}
