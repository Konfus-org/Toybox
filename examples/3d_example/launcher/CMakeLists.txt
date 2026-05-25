add_executable(ThreeDExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(ThreeDExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(ThreeDExampleLauncher PRIVATE
    Tbx::DefaultPlugins
    Tbx::Examples::ThreeDExampleRuntime
)
