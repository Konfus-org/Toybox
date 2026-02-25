add_executable(InputExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(InputExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(InputExampleLauncher PRIVATE
    Tbx::DefaultIncludes
    Tbx::Examples::InputExampleRuntime
)
