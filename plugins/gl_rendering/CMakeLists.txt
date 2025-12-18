add_library(GlRenderingPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(GlRenderingPlugin PRIVATE ${SRCS})
target_include_directories(GlRenderingPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(GlRenderingPlugin PRIVATE
    Tbx::App
    Tbx::FileSystem
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET GlRenderingPlugin
    CLASS tbx::plugins::GlRenderingPlugin
    HEADER "spd_file_logger_plugin.h"
    NAME "SpdFileLoggerPlugin"
    VERSION "1.0.0"
    TYPE "logger"
)

add_library(Tbx::Plugins::GlRenderingPlugin ALIAS GlRenderingPlugin)
