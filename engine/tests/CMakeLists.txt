# The engine tests own googletest. It must be static on Windows/clang to avoid DLL export
# issues, and built against the dynamic CRT to match the engine.
include(FetchContent)
if(NOT TARGET gtest)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    set(_gtest_had_shared FALSE)
    if(DEFINED BUILD_SHARED_LIBS)
        set(_gtest_had_shared TRUE)
        set(_gtest_prev_shared "${BUILD_SHARED_LIBS}")
    endif()
    set(BUILD_SHARED_LIBS OFF)

    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(googletest)

    if(_gtest_had_shared)
        set(BUILD_SHARED_LIBS "${_gtest_prev_shared}")
    else()
        unset(BUILD_SHARED_LIBS)
    endif()
endif()

include(engine_test_targets)

# Engine unit tests. Exercises engine-registered types (e.g. Transform) directly against the Engine
# DLL, so no per-test attribute codegen is needed. All *.cpp in this directory build into one binary.
tbx_add_engine_test(NAME EngineTests)
