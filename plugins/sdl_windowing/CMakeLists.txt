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
    SDL2::SDL2
)

add_library(Tbx::Plugins::SdlWindowingPlugin ALIAS SdlWindowingPlugin)
