#ifndef _TEST_HEADER_
#define _TEST_HEADER_

#include "CommonHead.h"
#include "ListConfigIni/list_ini.h"
#include "SetWinSound/SetWinSound.h"

namespace TEST
{
    void test_list_ini()
    {
        list_ini a;
        std::wstring file_name = L"C:\\code\\Blog\\build\\CommonTools\\RegProtectConfig.ini";
        a.parse(file_name);

        std::map<std::wstring, std::vector<std::wstring>> ini_data = a.get_ini_data();
        for (auto it = ini_data.begin(); it != ini_data.end(); it++)
        {
            std::wstring key = it->first;
            std::vector<std::wstring> value = it->second;
            std::wcout << key << std::endl;
            for (auto it2 = value.begin(); it2 != value.end(); it2++)
            {
                std::wcout << *it2 << std::endl;
            }
        }

        //解析完成后删除文件
        if(DeleteFile(file_name.c_str()))
        {
            printf("delete file success\n");
        }
        else
        {
            printf("delete file failed\n");
        }
    }

    void test_set_win_sound() 
    {

        auto set_win_sound = std::make_shared<SetWinSound>();
        
        auto res = set_win_sound->get_audio_devices();
        for (auto item:res)
        {
            LOGGER_INFO("");
        }

    }
}



#endif // _TEST_HEADER_