#pragma once

#ifndef _LOGGER_H_
# define _LOGGER_H_

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
//#include "boost/locale.hpp"

void init_logger(const char* log_file);

#define LOGGER_TRACE(...)		SPDLOG_TRACE(__VA_ARGS__)
#define LOGGER_DEBUG(...)		SPDLOG_DEBUG(__VA_ARGS__)
#define LOGGER_INFO(...)		SPDLOG_INFO(__VA_ARGS__)
#define LOGGER_WARN(...)		SPDLOG_WARN(__VA_ARGS__)
#define LOGGER_ERROR(...)		SPDLOG_ERROR(__VA_ARGS__)
#define LOGGER_CRITICAL(...)	SPDLOG_CRITICAL(__VA_ARGS__)

#define LOGGER_SERVICE(...)     spdlog::get("serv")->info(## __VA_ARGS__);

//#define A2UW(x)     (boost::locale::conv::to_utf<wchar_t>(x, "GBK").c_str())
//#define UW2A(x)     (boost::locale::conv::from_utf(x, "GBK").c_str())

#include <codecvt>

#define A2U(str)  to_wide_string(str)
#define U2A(str)  to_byte_string(str)

inline std::wstring to_wide_string(const std::string& input)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(input);
}

// convert wstring to string 
inline std::string to_byte_string(const std::wstring& input)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    //std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(input);
}

#endif /* !LOGGER_H_ */