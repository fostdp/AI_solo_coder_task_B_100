#pragma once

#include <string>

namespace fenyun {

enum class LogLevel {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    err = 4,
    critical = 5,
    off = 6
};

struct LoggerConfig {
    LogLevel level = LogLevel::info;
    std::string log_dir = "./logs";
    std::string file_prefix = "fenyun";
    size_t max_file_size_mb = 50;
    size_t max_files = 10;
    bool console_output = true;
    bool file_output = true;
    bool async_mode = true;
    size_t async_thread_count = 1;
    size_t async_queue_size = 8192;
};

bool init_logger(const LoggerConfig& cfg);
void shutdown_logger();
void set_log_level(LogLevel level);

}

#ifdef FENYUN_USE_SPDLOG

#include <spdlog/spdlog.h>

#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

#else

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fenyun_detail {
inline std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}
}

#define LOG_TRACE(...)   do { std::cout << "[TRACE " << fenyun_detail::timestamp() << "] " << __VA_ARGS__ << std::endl; } while(0)
#define LOG_DEBUG(...)   do { std::cout << "[DEBUG " << fenyun_detail::timestamp() << "] " << __VA_ARGS__ << std::endl; } while(0)
#define LOG_INFO(...)    do { std::cout << "[INFO  " << fenyun_detail::timestamp() << "] " << __VA_ARGS__ << std::endl; } while(0)
#define LOG_WARN(...)    do { std::cerr << "[WARN  " << fenyun_detail::timestamp() << "] " << __VA_ARGS__ << std::endl; } while(0)
#define LOG_ERROR(...)   do { std::cerr << "[ERROR " << fenyun_detail::timestamp() << "] " << __VA_ARGS__ << std::endl; } while(0)
#define LOG_CRITICAL(...) do { std::cerr << "[CRIT  " << fenyun_detail::timestamp() << "] " << __VA_ARGS__ << std::endl; } while(0)

#endif
