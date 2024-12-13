#pragma once

#ifndef _LOGGER_H_
# define _LOGGER_H_

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

void init_logger(const char* log_file);

#define LOGGER_TRACE(...)		SPDLOG_TRACE(__VA_ARGS__)
#define LOGGER_DEBUG(...)		SPDLOG_DEBUG(__VA_ARGS__)
#define LOGGER_INFO(...)		SPDLOG_INFO(__VA_ARGS__)
#define LOGGER_WARN(...)		SPDLOG_WARN(__VA_ARGS__)
#define LOGGER_ERROR(...)		SPDLOG_ERROR(__VA_ARGS__)
#define LOGGER_CRITICAL(...)	SPDLOG_CRITICAL(__VA_ARGS__)

#define LOGGER_SERVICE(...)     spdlog::get("serv")->info(## __VA_ARGS__);

#define A2U(str)  to_wide_string(str)
#define U2A(str)  to_byte_string(str)

inline std::string to_byte_string(const std::wstring& ws)
{
    if (ws.empty())  return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.data(), ws.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), ws.size(),&result[0], size, nullptr, nullptr);
    return result;
}

inline std::wstring to_wide_string(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> buf(size);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), size);
    return std::wstring(buf.data());
}

#endif /* !LOGGER_H_ */