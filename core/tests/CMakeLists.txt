add_subdirectory(app)
add_subdirectory(assets)
add_subdirectory(async)
add_subdirectory(files)
add_subdirectory(math)
add_subdirectory(messaging)
add_subdirectory(input)
add_subdirectory(plugin_api)
add_subdirectory(ecs)
add_subdirectory(graphics)
add_subdirectory(time)

include(test_output)

add_executable(TbxCoreUtilityTests
    "${CMAKE_CURRENT_SOURCE_DIR}/pch.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/string_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/pipeline_tests.cpp"
)
tbx_set_test_output(TbxCoreUtilityTests)
target_precompile_headers(TbxCoreUtilityTests PRIVATE "pch.h")
target_include_directories(TbxCoreUtilityTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(TbxCoreUtilityTests PRIVATE
    Tbx::Core
    gtest
    gtest_main
    gmock
)
add_test(NAME TbxCoreUtilityTests COMMAND $<TARGET_FILE:TbxCoreUtilityTests>)
