add_library(SdlAdapterPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl_adapter_plugin.cpp
)

target_include_directories(SdlAdapterPlugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SdlAdapterPlugin PRIVATE
    Engine
    SDL3::SDL3
)

tbx_register_plugin(
    TARGET SdlAdapterPlugin
    CLASS tbx::plugins::sdladapter::SdlAdapterPlugin
    HEADER "tbx/plugins/sdladapter/sdl_adapter_plugin.h"
    ENTRY CreateSdlAdapterPlugin
    ID "System.SDL"
    NAME "SDL Adapter"
    VERSION "1.0.0"
    TYPE "system"
)

add_library(Tbx::Plugins::SdlAdapterPlugin ALIAS SdlAdapterPlugin)
