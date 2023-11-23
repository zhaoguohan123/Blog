#ifndef _UTILS_READ_
#define _UTILS_READ_
#include "CommonHead.h"

namespace utils 
{
    /* 根据进程名称查找进程ID */
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

        // fix me: 映入boost库之后将宽字节转成多字节输出日志
        LOGGER_ERROR("Not found {} process", "zgh");

        if (hsnapshot)
        {
            CloseHandle(hsnapshot);
            hsnapshot = NULL;
        }
        return 0;
    }

    /*获取exe所在的目录*/
    std::string GetExePath()
    {
        char szFilePath[MAX_PATH + 1] = { 0 };
        GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
        /*
        strrchr:函数功能：查找一个字符c在另一个字符串str中末次出现的位置（也就是从str的右侧开始查找字符c首次出现的位置），
        并返回这个位置的地址。如果未能找到指定字符，那么函数将返回NULL。
        使用这个地址返回从最后一个字符c到str末尾的字符串。
        */
        (strrchr(szFilePath, '\\'))[0] = 0; // 删除文件名，只获得路径字串//
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
}
#endif