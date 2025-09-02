add_executable(Tests)

target_compile_features(Tests PRIVATE cxx_std_20)

target_precompile_headers(Tests PRIVATE "PCH.h")

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_sources(Tests PRIVATE ${TEST_SOURCES})

target_include_directories(Tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Tests PRIVATE
    Engine
    gtest
    gmock
)

add_test(NAME EngineTests COMMAND Tests)
