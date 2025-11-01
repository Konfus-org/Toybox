add_library(SdlAdapterPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sdl_adapter_plugin.cpp
)

target_link_libraries(SdlAdapterPlugin PRIVATE
    Engine
    SDL3::SDL3
)

add_custom_command(TARGET SdlAdapterPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/plugin.meta
            $<TARGET_FILE_DIR:SdlAdapterPlugin>/SdlAdapterPlugin.meta)

add_library(Tbx::Plugins::SdlAdapterPlugin ALIAS SdlAdapterPlugin)
