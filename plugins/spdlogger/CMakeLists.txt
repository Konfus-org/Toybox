add_library(SpdLoggerPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SpdLoggerPlugin PRIVATE ${SRCS})
target_include_directories(SpdLoggerPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SpdLoggerPlugin PRIVATE
    Tbx::Engine
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET SpdLoggerPlugin
    CLASS tbx::plugins::spdlogger::SpdLoggerPlugin
    HEADER "spdlogger_plugin.h"
    NAME "SpdLoggerPlugin"
    VERSION "1.0.0"
    TYPE "logger"
)

add_library(Tbx::Plugins::SpdLoggerPlugin ALIAS SpdLoggerPlugin)

