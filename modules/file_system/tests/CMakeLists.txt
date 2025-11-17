set(CMAKE_FOLDER "Tests")

add_executable(TbxFileSystemTests)

target_precompile_headers(TbxFileSystemTests PRIVATE "pch.h")

set(TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../std/tests/string_path_operations_tests.cpp"
)

target_sources(TbxFileSystemTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxFileSystemTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../std/tests"
)

set_target_properties(TbxFileSystemTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

target_link_libraries(TbxFileSystemTests PRIVATE
    Tbx::FileSystem
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxFileSystemTests COMMAND $<TARGET_FILE:TbxFileSystemTests>)
