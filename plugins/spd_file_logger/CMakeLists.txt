add_library(SpdFileLoggerPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SpdFileLoggerPlugin PRIVATE ${SRCS})
target_include_directories(SpdFileLoggerPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SpdFileLoggerPlugin PRIVATE
    Tbx::App
    Tbx::Files
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET SpdFileLoggerPlugin
    CLASS tbx::plugins::SpdFileLoggerPlugin
    HEADER "spd_file_logger_plugin.h"
    NAME "SpdFileLoggerPlugin"
    VERSION "1.0.0"
    CATEGORY "logging"
)

add_library(Tbx::Plugins::SpdFileLoggerPlugin ALIAS SpdFileLoggerPlugin)
