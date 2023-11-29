#ifndef _UTILS_READ_
#define _UTILS_READ_
#include "CommonHead.h"

namespace utils 
{
    /* 根据进程名获取进程ID */
    DWORD GetProcIdFromProcName(WCHAR* name)
    {
        HANDLE  hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hsnapshot == INVALID_HANDLE_VALUE)
        {
            LOGGER_ERROR("CreateToolhelp32Snapshot Error!");
            return 0;
        }

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        int flag = Process32First(hsnapshot, &pe);

        while (flag != 0)
        {
            if (_tcscmp(pe.szExeFile, name) == 0)
            {
                return pe.th32ProcessID;
            }
            flag = Process32Next(hsnapshot, &pe);
        }

        // fix me: 引入boost库后，将宽字符转换为多字符
        LOGGER_ERROR("Not found {} process", "zgh");

        if (hsnapshot)
        {
            CloseHandle(hsnapshot);
            hsnapshot = NULL;
        }
        return 0;
    }

    /*获取二进制所在路径*/
    std::string GetExePath()
    {
        char szFilePath[MAX_PATH + 1] = { 0 };
        GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
        /*
        strrchr:函数功能：查找一个字符c在另一个字符串str中末次出现的位置（也就是从str的右侧开始查找字符c首次出现的位置），
        并返回这个位置的地址。如果未能找到指定字符，那么函数将返回NULL。
        使用这个地址返回从最后一个字符c到str末尾的字符串。
        */
        (strrchr(szFilePath, '\\'))[0] = 0; //  删除文件名，只获得路径字串//
        std::string path = szFilePath;
        return path;
    }


    /*类似 3.2.1.2569 点分十进制的字符串比较*/
    //比较两个版本v1=v2 返回0，v1>v2返回1，v1<v2返回-1
    int compareVersions(const std::string& v1, const std::string& v2) {
        std::istringstream iss1(v1);
        std::istringstream iss2(v2);
        std::string token1, token2;

        while (std::getline(iss1, token1, '.') && std::getline(iss2, token2, '.')) {
            int num1 = std::stoi(token1);
            int num2 = std::stoi(token2);

            if (num1 < num2)
            {
                return -1;
            }

            if (num1 > num2)
            {
                return 1;
            }

        }
        return 0; // 两个版本号相等
    }

    
     /*判断文件是否存在： 1）注意szPath 需要传入绝对路径*/
    BOOL FileExists(LPCTSTR szPath)
    {
        DWORD dwAttrib = GetFileAttributes(szPath);

        return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    /*windows 判断盘符是否存在*/
    BOOL IsDriveExists(const wchar_t driveLetter) 
    {
        DWORD drivesMask = GetLogicalDrives();
        if(drivesMask == 0) {
            LOGGER_ERROR("GetLogicalDrives failed!errcode : {}", GetLastError());
            return FALSE;
        }
        int driveIndex = static_cast<int>(driveLetter - L'A');

        if ((drivesMask >> driveIndex) & 1) 
        {
            return TRUE; // 盘符存在
        }
         else 
        {
            return FALSE; // 盘符不存在
        }
    }

    /*根据进程名判断进程是否存在*/
    BOOL is_process_running(TCHAR * processName)
    {
        BOOL bRet = FALSE;
        HANDLE hSnapshot = NULL;
        PROCESSENTRY32 pe = { 0 };
        pe.dwSize = sizeof(pe);
        do
        {
            hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot == INVALID_HANDLE_VALUE)
            {
                LOGGER_ERROR("CreateToolhelp32Snapshot error: {} ", GetLastError());
                break;
            }

            if (!Process32First(hSnapshot, &pe))
            {
                LOGGER_ERROR("Process32First error: {}", GetLastError());
                break;
            }

            do
            {
                if (_wcsicmp(pe.szExeFile, processName) == 0)
                {
                    bRet = TRUE;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe));

        } while (false);
        
        if (hSnapshot)
        {
            CloseHandle(hSnapshot);
            hSnapshot = NULL;
        }
        
        return bRet;
    }

    /*打开服务*/
    BOOL start_serv(const std::string & strSrvName)
    {
        BOOL bRet = false;
        SC_HANDLE hSC = NULL;
        SC_HANDLE hService = NULL;

        do
        {
            hSC = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
            if (hSC == NULL)
            {
                LOGGER_ERROR("OpenSCManagerA failed! errcode {}", GetLastError());
                break;
            }

            hService = OpenServiceA(hSC, strSrvName.c_str(), SERVICE_ALL_ACCESS);
            if (hService == NULL)
            {
                LOGGER_ERROR("OpenService %s failed! errcode {}", strSrvName.c_str(), GetLastError());
                break;
            }

            if (!StartService(hService, 0, NULL))
            {
                LOGGER_ERROR("StartService failed! errcode {}", GetLastError());
                break;
            }
            bRet = TRUE;
        } while (FALSE);

        if (hService)
        {
            CloseServiceHandle(hService);
            hService = NULL;
        }
        if (hSC)
        {
            CloseServiceHandle(hSC);
            hSC = NULL;
        }

        return bRet;
    }
}
#endif