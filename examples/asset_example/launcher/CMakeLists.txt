add_executable(AssetExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(AssetExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(AssetExampleLauncher PRIVATE
    Tbx::App
    Tbx::Examples::AssetExampleRuntime
)

