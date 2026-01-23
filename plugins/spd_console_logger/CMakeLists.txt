tbx_register_plugin(
    TARGET SpdConsoleLoggerPlugin
    CLASS tbx::plugins::SpdConsoleLoggerPlugin
    HEADER "spd_console_logger_plugin.h"
    NAME "SpdConsoleLoggerPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

target_link_libraries(SpdConsoleLoggerPlugin PRIVATE
    Tbx::App
    Tbx::Files
    spdlog::spdlog
)

add_library(Tbx::Plugins::SpdConsoleLoggerPlugin ALIAS SpdConsoleLoggerPlugin)
