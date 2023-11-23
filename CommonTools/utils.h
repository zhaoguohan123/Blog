#ifndef _UTILS_READ_
#define _UTILS_READ_
#include "CommonHead.h"

namespace utils 
{
    /* ���ݽ������Ʋ��ҽ���ID */
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

        // fix me: ӳ��boost��֮�󽫿��ֽ�ת�ɶ��ֽ������־
        LOGGER_ERROR("Not found {} process", "zgh");

        if (hsnapshot)
        {
            CloseHandle(hsnapshot);
            hsnapshot = NULL;
        }
        return 0;
    }

    /*��ȡexe���ڵ�Ŀ¼*/
    std::string GetExePath()
    {
        char szFilePath[MAX_PATH + 1] = { 0 };
        GetModuleFileNameA(NULL, szFilePath, MAX_PATH);
        /*
        strrchr:�������ܣ�����һ���ַ�c����һ���ַ���str��ĩ�γ��ֵ�λ�ã�Ҳ���Ǵ�str���Ҳ࿪ʼ�����ַ�c�״γ��ֵ�λ�ã���
        ���������λ�õĵ�ַ�����δ���ҵ�ָ���ַ�����ô����������NULL��
        ʹ�������ַ���ش����һ���ַ�c��strĩβ���ַ�����
        */
        (strrchr(szFilePath, '\\'))[0] = 0; // ɾ���ļ�����ֻ���·���ִ�//
        std::string path = szFilePath;
        return path;
    }


    /*���� 3.2.1.2569 ���ʮ���Ƶ��ַ����Ƚ�*/
    //�Ƚ������汾v1=v2 ����0��v1>v2����1��v1<v2����-1
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
        return 0; // �����汾�����
    }

    
    /*�ж��ļ��Ƿ���ڣ� 1��ע��szPath ��Ҫ�������·��*/
    BOOL FileExists(LPCTSTR szPath)
    {
        DWORD dwAttrib = GetFileAttributes(szPath);

        return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }
}
#endif