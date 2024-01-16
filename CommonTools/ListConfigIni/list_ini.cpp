#include "list_ini.h"

list_ini::list_ini()
{
}

list_ini::~list_ini()
{
}

BOOL list_ini::parse(std::wstring &file_name)
{
    BOOL bRet = FALSE;

    do
    {
        if (file_name.empty())
        {
            printf("file_name is empty \n");
            break;
        }
        
        // 获取所有节的名称
        WCHAR sections[4096] ={0};
        DWORD sectionsSize = GetPrivateProfileSectionNames(sections, sizeof(sections) / sizeof(WCHAR), file_name.c_str());
        if (sectionsSize == 0) 
        {
            printf("Error reading sections from INI file. error code: %u \n", GetLastError());
            break;
        }
        
        // 遍历每个节
        WCHAR* currentSection = sections;
        while (*currentSection != L'\0') {
            
            // 获取当前节的所有键值
            WCHAR keyValues[4096]={0};
            DWORD keyValuesSize = GetPrivateProfileString(currentSection, NULL, L"", keyValues, sizeof(keyValues) / sizeof(WCHAR), file_name.c_str());
            std::vector<std::wstring> paths;
            if (keyValuesSize > 0)
            {
                // 遍历每个键值
                WCHAR* currentKeyValue = keyValues;
                 
                while (*currentKeyValue != L'\0') {
                    // 获取键的值
                    WCHAR value[256];
                    GetPrivateProfileString(currentSection, currentKeyValue, L"", value, sizeof(value) / sizeof(WCHAR), file_name.c_str());

                    paths.push_back(value);
                    // 移动到下一个键值对
                    currentKeyValue += wcslen(currentKeyValue) + 1;
                }
            }
            ini_data_.insert(std::make_pair(currentSection,paths));
            // 移动到下一个节
            currentSection += wcslen(currentSection) + 1;
        }
        
        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

std::map<std::wstring, std::vector<std::wstring>> list_ini::get_ini_data()
{
    return ini_data_;
}
