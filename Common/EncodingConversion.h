#pragma once

/*
Windows环境中，char默认使用ANSI编码，wchar_t默认使用UTF16编码

以下str使用的是ANSI:
char* str = "你好";
以下wstr使用的是UTF16:
wchar_t* wstr = L"你好"; 

其中str的编码会随着系统设置的编码活动页影响，或者也与编译器中的一些设置有关
所以最好在windows设置中，通过以下方式，将utf8支持打开：

"区域设置"->"其他日期、时间和区域设置"->"更改日期、时间或数字格式"->
"管理"->"更改系统区域设置"->勾选"Beta版：使用Unicode UTF-8 提供全球语言支持"
随后确定并重启
*/

#ifndef _ENCODING_CONVERSION_H_
#define _ENCODING_CONVERSION_H_

#include <Windows.h>
#include <vector>
#include <string>


/// @brief 将ANSI编码的窄字符串转换为UTF16编码的宽字符串
/// @param strANSI ANSI编码格式的窄字符串
/// @return UTF16编码格式的宽字符串
std::wstring ANSIToUTF16(const std::string strANSI);
/// @brief 将UTF16编码的宽字符串转换为ANSI编码的窄字符串
/// @param strUTF16 UTF16编码格式的宽字符串
/// @return ANSI编码格式的窄字符串
std::string UTF16ToANSI(const std::wstring strUTF16);

/// @brief 将UTF8编码的窄字符串转换为UTF16编码的宽字符串
/// @param strUTF8 UTF8编码格式的窄字符串
/// @return UTF16编码格式的宽字符串
std::wstring UTF8ToUTF16(const std::string strUTF8);
/// @brief 将UTF16编码的宽字符串转换为UTF8编码的窄字符串
/// @param strUTF16 UTF16编码格式的宽字符串
/// @return UTF8编码格式的窄字符串
std::string UTF16ToUTF8(const std::wstring strUTF16);

/// @brief 将ANSI编码的窄字符串转换为UTF8编码的窄字符串
/// @param strANSI ANSI编码格式的窄字符串
/// @return UTF8编码格式的窄字符串
std::string ANSIToUTF8(const std::string strANSI);
/// @brief 将UTF8编码的窄字符串转换为ANSI编码的窄字符串
/// @param strUTF8 UTF8编码格式的窄字符串
/// @return ANSI编码格式的窄字符串
std::string UTF8ToANSI(const std::string strUTF8);


/// @brief 根据编码将宽字符串转成窄字符串
/// @attention code_page为窄字符串的编码，宽字符串固定为UTF16编码
/// @param wstr 要转换的宽字符串
/// @param code_page 转换后的窄字符串的编码格式
/// @return code_page编码格式的窄字符串
std::string wstring2string(const std::wstring wstr, uint32_t code_page);
/// @brief 根据编码将窄字符串转成宽字符串
/// @attention code_page为窄字符串的编码，宽字符串固定为UTF16编码
/// @param str 要转换的窄字符串
/// @param code_page 要转换的窄字符串的编码格式
/// @return UTF16编码格式的宽字符串
std::wstring string2wstring(const std::string str, uint32_t code_page);


inline std::wstring ANSIToUTF16(const std::string strANSI) {
    return string2wstring(strANSI, CP_ACP);
}
inline std::string UTF16ToANSI(const std::wstring strUTF16) {
    return wstring2string(strUTF16, CP_ACP);
}

inline std::wstring UTF8ToUTF16(const std::string strUTF8) {
    return string2wstring(strUTF8, CP_UTF8);
}
inline std::string UTF16ToUTF8(const std::wstring strUTF16) {
    return wstring2string(strUTF16, CP_UTF8);
}

inline std::string ANSIToUTF8(const std::string strANSI) {
    return wstring2string(string2wstring(strANSI, CP_ACP), CP_UTF8);
}
inline std::string UTF8ToANSI(const std::string strUTF8) {
    return wstring2string(string2wstring(strUTF8, CP_UTF8), CP_ACP);
}


inline std::string wstring2string(const std::wstring wstr, uint32_t code_page) {
    if (wstr.empty())
        return std::string();

    // Compute the length of the buffer we'll need.
    int wstrlen = static_cast<int>(wstr.length());
    int charcount = WideCharToMultiByte(code_page, 0, wstr.data(), wstrlen, NULL, 0, NULL, NULL);
    if (charcount == 0)
        return std::string();

    std::string str;
    str.resize(static_cast<size_t>(charcount));
    WideCharToMultiByte(code_page, 0, wstr.data(), wstrlen, &str[0], charcount, NULL, NULL);
    return str;
}
inline std::wstring string2wstring(const std::string str, uint32_t code_page) {
    if (str.empty())
        return std::wstring();
    
    // Compute the length of the buffer.
    int strlen = static_cast<int>(str.length());
    int charcount = MultiByteToWideChar(code_page, 0, str.data(), strlen, NULL, 0);
    if (charcount == 0)
        return std::wstring();

    std::wstring wstr;
    wstr.resize(static_cast<size_t>(charcount));
    MultiByteToWideChar(code_page, 0, str.data(), strlen, &wstr[0], charcount);
    return wstr;
}

#endif