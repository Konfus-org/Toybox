add_executable(PhysicsExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(PhysicsExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(PhysicsExampleLauncher PRIVATE
    Tbx::App
    Tbx::Examples::PhysicsExampleRuntime
)
