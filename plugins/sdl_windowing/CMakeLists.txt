set(plugin_name SdlWindowingPlugin)

add_library(${plugin_name} SHARED)
add_library(Tbx::Plugins::${plugin_name} ALIAS ${plugin_name})

target_include_directories(${plugin_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${plugin_name} PRIVATE
        Tbx::Engine
        EnTT::EnTT
        Tbx::Plugins::SdlBaseSystemsPlugin
        SDL3::SDL3-shared
)

tbx_register_plugin(
        TARGET ${plugin_name}
        CLASS sdl_windowing::SdlWindowingPlugin
        HEADER "tbx/plugins/sdl_windowing/sdl_windowing_plugin.h"
        NAME "${plugin_name}"
        VERSION "1.0.0"
        CATEGORY "input"
)
