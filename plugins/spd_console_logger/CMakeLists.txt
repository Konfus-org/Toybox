add_library(SpdConsoleLoggerPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources(SpdConsoleLoggerPlugin PRIVATE ${SRCS})
target_include_directories(SpdConsoleLoggerPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SpdConsoleLoggerPlugin PRIVATE
    Tbx::App
    Tbx::Files
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET SpdConsoleLoggerPlugin
    CLASS tbx::plugins::SpdConsoleLoggerPlugin
    HEADER "spd_console_logger_plugin.h"
    NAME "SpdConsoleLoggerPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::SpdConsoleLoggerPlugin ALIAS SpdConsoleLoggerPlugin)

