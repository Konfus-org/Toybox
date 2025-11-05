add_library(SdlWindowingPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl_windowing_plugin.cpp
)

target_include_directories(SdlWindowingPlugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SdlWindowingPlugin PRIVATE
    Engine
    SDL3::SDL3-shared
    Tbx::Plugins::SdlAdapterPlugin
)

tbx_register_plugin(
    TARGET SdlWindowingPlugin
    CLASS tbx::plugins::sdlwindowing::SdlWindowingPlugin
    HEADER "tbx/plugins/sdlwindowing/sdl_windowing_plugin.h"
    NAME SdlWindowingPlugin
    VERSION "1.0.0"
    TYPE "windowing"
)

add_library(Tbx::Plugins::SdlWindowingPlugin ALIAS SdlWindowingPlugin)
