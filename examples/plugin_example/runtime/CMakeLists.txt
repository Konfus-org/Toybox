add_library(PluginExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(PluginExampleRuntime PRIVATE ${SRCS})
target_include_directories(PluginExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(PluginExampleRuntime PRIVATE
    Tbx::App
    Tbx::ECS
    Tbx::Plugins::SpdConsoleLoggerPlugin
    Tbx::Plugins::SdlWindowingPlugin
)

tbx_register_plugin(
    TARGET PluginExampleRuntime
    CLASS tbx::examples::ExampleRuntimePlugin
    HEADER "runtime_plugin.h"
    NAME "ExampleRuntimePlugin"
    VERSION "1.0.0"
    TYPE "runtime"
)

add_library(Tbx::Examples::PluginExampleRuntime ALIAS PluginExampleRuntime)
