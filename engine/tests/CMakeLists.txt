add_executable(Tests)

# PCH
target_precompile_headers(Tests PRIVATE "pch.h")

# Sources
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
target_sources(Tests PRIVATE ${TEST_SOURCES})
target_include_directories(Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# C++20
set_target_properties(Engine PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Links
target_link_libraries(Tests PRIVATE
    Engine
    gtest
    gtest_main
    gmock
)

# Add tests
add_test(NAME EngineTests COMMAND $<TARGET_FILE:Tests>)

# Build a small dynamic plugin used by tests
add_library(TestDynPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/test_plugins/dynamic/test_dyn_plugin.cpp
)
target_link_libraries(TestDynPlugin PRIVATE Engine)

# Pass the built path of the dynamic test plugin to the tests
target_compile_definitions(Tests PRIVATE TBX_TEST_DYN_PLUGIN_PATH="$<TARGET_FILE:TestDynPlugin>")
add_dependencies(Tests TestDynPlugin)
