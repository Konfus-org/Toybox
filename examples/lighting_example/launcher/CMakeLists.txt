add_executable(LightingExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(LightingExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(LightingExampleLauncher PRIVATE
    Tbx::DefaultIncludes
    Tbx::Examples::LightingExampleRuntime
)
