tbx_register_plugin(
    TARGET SdlWindowingPlugin
    CLASS tbx::plugins::SdlWindowingPlugin
    HEADER "sdl_windowing_plugin.h"
    NAME "SdlWindowingPlugin"
    VERSION "1.0.0"
    CATEGORY "rendering"
)

target_link_libraries(SdlWindowingPlugin PRIVATE
    Tbx::App
    SDL3::SDL3-shared
)

add_library(Tbx::Plugins::SdlWindowingPlugin ALIAS SdlWindowingPlugin)
