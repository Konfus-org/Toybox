add_executable(TwoDExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(TwoDExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(TwoDExampleLauncher PRIVATE
    Tbx::DefaultPlugins
    Tbx::Examples::TwoDExampleRuntime
)
