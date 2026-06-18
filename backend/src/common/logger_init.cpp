#include "common/logger_init.h"

#ifdef FENYUN_USE_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#endif

#include <iostream>
#include <vector>
#include <memory>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace fenyun {

static spdlog::level::level_enum to_spdlog_level(LogLevel l) {
    switch (l) {
        case LogLevel::trace:    return spdlog::level::trace;
        case LogLevel::debug:    return spdlog::level::debug;
        case LogLevel::info:     return spdlog::level::info;
        case LogLevel::warn:     return spdlog::level::warn;
        case LogLevel::err:      return spdlog::level::err;
        case LogLevel::critical: return spdlog::level::critical;
        default:                 return spdlog::level::off;
    }
}

bool init_logger(const LoggerConfig& cfg) {
#ifdef FENYUN_USE_SPDLOG
    try {
        if (cfg.async_mode) {
            spdlog::init_thread_pool(cfg.async_queue_size, cfg.async_thread_count);
        }

        std::vector<spdlog::sink_ptr> sinks;

        if (cfg.console_output) {
            auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console->set_level(to_spdlog_level(cfg.level));
            console->set_pattern("[%Y-%m-%dT%H:%M:%S.%e] [%^%l%$] [%t] %v");
            sinks.push_back(console);
        }

        if (cfg.file_output) {
            MKDIR(cfg.log_dir.c_str());
            std::string file_path = cfg.log_dir + "/" + cfg.file_prefix + ".log";
            auto file = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                file_path,
                cfg.max_file_size_mb * 1024ULL * 1024ULL,
                cfg.max_files,
                false
            );
            file->set_level(to_spdlog_level(cfg.level));
            file->set_pattern("[%Y-%m-%dT%H:%M:%S.%e] [%l] [%t] %v");
            sinks.push_back(file);
        }

        auto logger = cfg.async_mode
            ? std::make_shared<spdlog::async_logger>(
                "fenyun", sinks.begin(), sinks.end(),
                spdlog::thread_pool(), spdlog::async_overflow_policy::block)
            : std::make_shared<spdlog::logger>(
                "fenyun", sinks.begin(), sinks.end());

        logger->set_level(to_spdlog_level(cfg.level));
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(3));
        spdlog::flush_on(to_spdlog_level(LogLevel::warn));

        SPDLOG_INFO("Logger initialized: level={}, async={}, sinks={}",
                    cfg.level, cfg.async_mode, sinks.size());
        return true;
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "[logger_init FAILED] " << ex.what() << std::endl;
        return false;
    }
#else
    (void)cfg;
    std::cout << "[logger_init] spdlog disabled - using std::cout fallback" << std::endl;
    return true;
#endif
}

void shutdown_logger() {
#ifdef FENYUN_USE_SPDLOG
    SPDLOG_INFO("Logger shutting down");
    spdlog::drop_all();
    spdlog::shutdown();
#endif
}

void set_log_level(LogLevel level) {
#ifdef FENYUN_USE_SPDLOG
    spdlog::set_level(to_spdlog_level(level));
#endif
}

}
