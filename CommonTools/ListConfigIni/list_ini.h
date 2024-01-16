#ifndef _LIST_INI_HEADER_
#define _LIST_INI_HEADER_
#include "CommonHead.h"

class list_ini
{
public:
    list_ini();
    ~list_ini();

    BOOL parse(std::wstring &file_name);
    std::map<std::wstring, std::vector<std::wstring>> get_ini_data();

private:
    std::map<std::wstring, std::vector<std::wstring>> ini_data_;
};

#endif // _LIST_INI_HEADER_