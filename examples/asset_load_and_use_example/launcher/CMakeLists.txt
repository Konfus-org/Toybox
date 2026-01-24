add_executable(AssetLoadAndUseExampleLauncher
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(AssetLoadAndUseExampleLauncher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(AssetLoadAndUseExampleLauncher PRIVATE
    Tbx::App
    Tbx::Examples::AssetLoadAndUseExampleRuntime
)
