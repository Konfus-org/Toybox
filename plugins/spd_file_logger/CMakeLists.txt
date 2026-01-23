tbx_register_plugin(
    TARGET SpdFileLoggerPlugin
    CLASS tbx::plugins::SpdFileLoggerPlugin
    HEADER "spd_file_logger_plugin.h"
    NAME "SpdFileLoggerPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

target_link_libraries(SpdFileLoggerPlugin PRIVATE
    Tbx::App
    Tbx::Files
    spdlog::spdlog
)

add_library(Tbx::Plugins::SpdFileLoggerPlugin ALIAS SpdFileLoggerPlugin)
