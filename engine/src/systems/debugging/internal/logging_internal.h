#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/debugging/logging.h"
#include <functional>
#include <memory>
#include <mutex>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog-inl.h>
#include <string>
#include <unordered_set>

namespace tbx::internal
{
    static std::mutex logger_mutex = {};
    static std::mutex once_mutex = {};
    static std::shared_ptr<spdlog::logger> logger = {};
    static std::unordered_set<size_t> once_message_hashes = {};

    static std::shared_ptr<spdlog::logger> create_default_logger()
    {
        auto file_operator = FileOperator();
        auto logs_directory = file_operator.resolve("logs");
        auto path = file_operator.rotate(logs_directory, "TbxDebug", ".log", 10);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef TBX_PLATFORM_WINDOWS
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        return std::make_shared<spdlog::logger>(
            "Toybox",
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
                msvc_sink,
            });
#else
        return std::make_shared<spdlog::logger>(
            "Toybox",
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
            });
#endif
    }

    static std::shared_ptr<spdlog::logger> get_or_create_default_logger()
    {
        std::lock_guard<std::mutex> lock(logger_mutex);
        if (logger)
            return logger;

        logger = create_default_logger();
        return logger;
    }

}
