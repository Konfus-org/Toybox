add_library(SpdConsoleLoggerPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SpdConsoleLoggerPlugin PRIVATE ${SRCS})
target_include_directories(SpdConsoleLoggerPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SpdConsoleLoggerPlugin PRIVATE
    Tbx::App
    Tbx::FileSystem
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET SpdConsoleLoggerPlugin
    CLASS tbx::plugins::spdconsolelogger::SpdConsoleLoggerPlugin
    HEADER "spd_console_logger_plugin.h"
    NAME "SpdConsoleLoggerPlugin"
    VERSION "1.0.0"
    TYPE "logger"
)

add_library(Tbx::Plugins::SpdConsoleLoggerPlugin ALIAS SpdConsoleLoggerPlugin)

