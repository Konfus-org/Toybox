add_library(PluginExampleRuntime SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.cpp
)

target_include_directories(PluginExampleRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(PluginExampleRuntime PRIVATE
    Tbx::Engine
    Tbx::Plugins::SpdLoggerPlugin
    Tbx::Plugins::SdlWindowingPlugin
)

tbx_register_plugin(
    TARGET PluginExampleRuntime
    CLASS tbx::examples::ExampleRuntimePlugin
    HEADER "tbx/examples/runtime_plugin.h"
    NAME ExampleRuntimePlugin
    VERSION "1.0.0"
    TYPE "runtime"
)

add_library(Tbx::Examples::PluginExampleRuntime ALIAS PluginExampleRuntime)
