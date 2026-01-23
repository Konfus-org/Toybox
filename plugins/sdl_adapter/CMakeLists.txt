tbx_register_plugin(
    TARGET SdlAdapterPlugin
    CLASS tbx::plugins::SdlAdapterPlugin
    HEADER "sdl_adapter_plugin.h"
    NAME "SdlAdapterPlugin"
    VERSION "1.0.0"
    CATEGORY "input"
)

target_link_libraries(SdlAdapterPlugin PRIVATE
    Tbx::App
    SDL2::SDL2
)

add_library(Tbx::Plugins::SdlAdapterPlugin ALIAS SdlAdapterPlugin)
