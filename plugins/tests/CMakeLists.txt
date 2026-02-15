set(CMAKE_FOLDER "tests")
add_executable(TbxImporterTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/plugins/*/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/shared/*.h"
)

target_precompile_headers(TbxImporterTests PRIVATE "pch.h")
target_sources(TbxImporterTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxImporterTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxImporterTests PRIVATE
    Tbx::Plugins::StbImageLoaderPlugin
    Tbx::Plugins::GlslShaderLoaderPlugin
    Tbx::Plugins::MatMaterialLoaderPlugin
    Tbx::Plugins::AssimpModelLoaderPlugin
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxImporterTests COMMAND $<TARGET_FILE:TbxImporterTests>)
unset(CMAKE_FOLDER)
