add_executable(EcsExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(EcsExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(EcsExampleLauncher PRIVATE
    Tbx::App
    Tbx::Examples::EcsExampleRuntime
)
