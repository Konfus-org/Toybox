add_library(PluginExampleRuntime SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime.cpp
)

target_include_directories(PluginExampleRuntime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(PluginExampleRuntime PRIVATE
    Tbx::Engine
    Tbx::Plugins::SpdLoggerPlugin
)

# Copy manifest next to the built library
add_custom_command(TARGET PluginExampleRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/plugin.meta
            $<TARGET_FILE_DIR:PluginExampleRuntime>/PluginExampleRuntime.meta)

add_library(Tbx::Examples::PluginExampleRuntime ALIAS PluginExampleRuntime)
