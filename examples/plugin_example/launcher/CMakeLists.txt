add_executable(PluginExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(PluginExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(PluginExampleLauncher PRIVATE
    Tbx::Core
    Tbx::Examples::PluginExampleRuntime
)
