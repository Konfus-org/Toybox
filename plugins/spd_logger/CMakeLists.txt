set(plugin_name SpdLoggerPlugin)

add_library(${plugin_name} SHARED)
add_library(Tbx::Plugins::${plugin_name} ALIAS ${plugin_name})

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(${plugin_name} PRIVATE ${SRCS})
target_include_directories(${plugin_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${plugin_name} PRIVATE
    Tbx::App
    Tbx::Files
    spdlog::spdlog
)

tbx_register_plugin(
    TARGET ${plugin_name}
    CLASS tbx::plugins::SpdLoggerPlugin
    HEADER "spd_logger_plugin.h"
    NAME "${plugin_name}"
    VERSION "1.0.0"
    CATEGORY "logging"
)
