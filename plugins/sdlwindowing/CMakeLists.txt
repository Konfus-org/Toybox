add_library(SdlWindowingPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl_windowing_plugin.cpp
)

target_link_libraries(SdlWindowingPlugin PRIVATE
    Engine
    SDL3::SDL3
)

add_custom_command(TARGET SdlWindowingPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/plugin.meta
            $<TARGET_FILE_DIR:SdlWindowingPlugin>/SdlWindowingPlugin.meta)

add_library(Tbx::Plugins::SdlWindowingPlugin ALIAS SdlWindowingPlugin)
