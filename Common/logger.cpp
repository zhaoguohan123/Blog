#include "logger.h"
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>


void init_logger(const char* log_file)
{
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, 1048576 * 10, 10);
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    spdlog::sinks_init_list sink_list = { file_sink, msvc_sink };
    auto logger = std::make_shared<spdlog::logger>("root", sink_list.begin(), sink_list.end());
    spdlog::set_default_logger(logger);

    // auto service_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    auto service_logger = std::make_shared<spdlog::logger>("serv", sink_list.begin(), sink_list.end());
    spdlog::register_logger(service_logger);

    spdlog::set_error_handler([](const std::string& msg) { LOGGER_ERROR("*** LOGGER ERROR ***: {}", msg); });

    //spdlog::flush_every(std::chrono::seconds(3));
    spdlog::flush_on(spdlog::level::info);
    spdlog::cfg::load_env_levels();
    // spdlog::set_level(spdlog::level::info);

    spdlog::set_pattern("[%Y-%m-%d %T.%F] [%n] [%l] [%P] [%t] %@ %v");
}
