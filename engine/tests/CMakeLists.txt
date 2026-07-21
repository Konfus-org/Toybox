if(NOT TARGET GTest::gtest_main)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2)
    FetchContent_MakeAvailable(googletest)
endif()

file(GLOB TBX_TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(tbx_tests ${TBX_TEST_SOURCES})
target_link_libraries(tbx_tests PRIVATE tbx GTest::gtest_main)
target_precompile_headers(tbx_tests PRIVATE
    <string> <vector> <memory> <optional>
    <tbx_math_backend.h> <tbx_serialization_backend.h> <gtest/gtest.h>)

tbx_copy_runtime_dlls(tbx_tests)

include(GoogleTest)
gtest_discover_tests(tbx_tests DISCOVERY_MODE PRE_TEST)
