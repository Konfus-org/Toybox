add_library(SpdLoggerPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spdlogger_plugin.cpp
)

target_include_directories(SpdLoggerPlugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SpdLoggerPlugin PRIVATE
    Engine
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET SpdLoggerPlugin
    CLASS tbx::plugins::spdlogger::SpdLoggerPlugin
    HEADER "tbx/plugins/spdlogger/spdlogger_plugin.h"
    ENTRY CreateSpdLoggerPlugin
    ID "Logging.SpdLogger"
    NAME "SpdLogger"
    VERSION "1.0.0"
    TYPE "logger"
)

add_library(Tbx::Plugins::SpdLoggerPlugin ALIAS SpdLoggerPlugin)

