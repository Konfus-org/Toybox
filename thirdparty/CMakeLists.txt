set(CMAKE_FOLDER "third party")

function(tbx_set_thirdparty_bool option_name option_value)
    set(${option_name} ${option_value} CACHE BOOL "" FORCE)
endfunction()

function(tbx_set_thirdparty_string option_name option_value)
    set(${option_name} ${option_value} CACHE STRING "" FORCE)
endfunction()

tbx_set_thirdparty_bool(BUILD_TESTING OFF)

tbx_set_thirdparty_bool(SDL_SHARED ON)
tbx_set_thirdparty_bool(SDL_STATIC OFF)
tbx_set_thirdparty_bool(SDL_TEST_LIBRARY OFF)
tbx_set_thirdparty_bool(SDL_TESTS OFF)
tbx_set_thirdparty_bool(SDL_EXAMPLES OFF)
tbx_set_thirdparty_bool(SDL_INSTALL OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    tbx_set_thirdparty_bool(SDL_WAYLAND ON)
    tbx_set_thirdparty_bool(SDL_X11 ON)
endif ()

tbx_set_thirdparty_string(IMGUI_PLATFORM SDL3)
tbx_set_thirdparty_string(IMGUI_RENDERER OpenGL3)
tbx_set_thirdparty_string(IMGUI_GL_LOADER GLAD)
tbx_set_thirdparty_bool(IMGUI_BUILD_DEMO OFF)
tbx_set_thirdparty_bool(SPDLOG_BUILD_TESTING OFF)
tbx_set_thirdparty_bool(SPDLOG_BUILD_EXAMPLES OFF)
tbx_set_thirdparty_bool(SPDLOG_BUILD_SHARED OFF)
tbx_set_thirdparty_bool(GLM_BUILD_LIBRARY OFF)
tbx_set_thirdparty_bool(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
tbx_set_thirdparty_bool(ASSIMP_BUILD_TESTS OFF)
tbx_set_thirdparty_bool(ASSIMP_INSTALL OFF)
tbx_set_thirdparty_bool(ASSIMP_INSTALL_PDB OFF)
tbx_set_thirdparty_bool(ASSIMP_BUILD_ZLIB ON)
tbx_set_thirdparty_bool(ASSIMP_WARNINGS_AS_ERRORS OFF)
tbx_set_thirdparty_bool(ASSIMP_BUILD_SAMPLES OFF)
tbx_set_thirdparty_bool(ASSIMP_BUILD_DOCS OFF)
tbx_set_thirdparty_bool(BENCHMARK_ENABLE_TESTING OFF)
tbx_set_thirdparty_bool(BENCHMARK_ENABLE_INSTALL OFF)
tbx_set_thirdparty_bool(JSON_BuildTests OFF)
tbx_set_thirdparty_bool(JSON_Install OFF)
function(tbx_add_thirdparty_library thirdparty_subdirectory)
    add_subdirectory("${thirdparty_subdirectory}")
    foreach (thirdparty_target IN LISTS ARGN)
        if (NOT TARGET ${thirdparty_target})
            continue()
        endif ()

        # Ensure third-party warnings never become build-stopping errors.
        set_property(TARGET ${thirdparty_target} PROPERTY COMPILE_WARNING_AS_ERROR OFF)

        # Unity build often breaks third party libs.
        set_property(TARGET ${thirdparty_target} PROPERTY UNITY_BUILD OFF)

        # Mark third-party include dirs as system so their warnings don\'t fail consumers.
        get_target_property(_tbx_iface_inc ${thirdparty_target} INTERFACE_INCLUDE_DIRECTORIES)
        if (_tbx_iface_inc)
            target_include_directories(${thirdparty_target} SYSTEM INTERFACE ${_tbx_iface_inc})
        endif ()
    endforeach ()
endfunction()

tbx_set_thirdparty_bool(ENABLE_ALL_WARNINGS OFF)

tbx_add_thirdparty_library(assimp assimp)
tbx_add_thirdparty_library(entt EnTT)
tbx_add_thirdparty_library(glad glad)
tbx_add_thirdparty_library(stbimg stbimg)
tbx_add_thirdparty_library(glm glm glm-header-only)
tbx_add_thirdparty_library(nlohmann_json nlohmann_json)
tbx_add_thirdparty_library(spdlog spdlog spdlog_header_only)
tbx_add_thirdparty_library(SDL SDL3-static SDL3-shared SDL3_test)
tbx_add_thirdparty_library(imgui imgui)

# Toybox uses Jolt only for CPU physics. Keep optional GPU compute backends out of
# the engine dependency graph so the runtime doesn't require SDK DLLs such as DXC.
tbx_set_thirdparty_bool(JPH_USE_DX12 OFF)
tbx_set_thirdparty_bool(JPH_USE_DXC OFF)
tbx_set_thirdparty_bool(JPH_USE_VK OFF)
tbx_set_thirdparty_bool(JPH_USE_MTL OFF)
tbx_set_thirdparty_bool(JPH_USE_CPU_COMPUTE ON)
tbx_set_thirdparty_bool(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
# Disable Jolt LTO to avoid static archive format incompatibilities when linked into shared plugins on Linux CI.
tbx_set_thirdparty_bool(INTERPROCEDURAL_OPTIMIZATION OFF)

set(tbx_had_build_shared_libs_cache_for_jolt OFF)
if (DEFINED CACHE{BUILD_SHARED_LIBS})
    set(tbx_had_build_shared_libs_cache_for_jolt ON)
    set(tbx_previous_build_shared_libs_for_jolt "${BUILD_SHARED_LIBS}")
endif ()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries (DLLs)." FORCE)
tbx_add_thirdparty_library(JoltPhysics/Build Jolt)

if (tbx_had_build_shared_libs_cache_for_jolt)
    set(BUILD_SHARED_LIBS "${tbx_previous_build_shared_libs_for_jolt}" CACHE BOOL "Build shared libraries (DLLs)." FORCE)
else ()
    unset(BUILD_SHARED_LIBS CACHE)
endif ()
unset(BUILD_SHARED_LIBS)

set(tbx_had_build_shared_libs_cache OFF)
if (DEFINED CACHE{BUILD_SHARED_LIBS})
    set(tbx_had_build_shared_libs_cache ON)
    set(tbx_previous_build_shared_libs "${BUILD_SHARED_LIBS}")
endif ()

# GoogleTest/GoogleMock need to be built as static libs on Windows/clang to avoid DLL export issues.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries (DLLs)." FORCE)
tbx_add_thirdparty_library(googletest gtest gtest_main gmock gmock_main)

unset(ENABLE_ALL_WARNINGS CACHE)

if (tbx_had_build_shared_libs_cache)
    set(BUILD_SHARED_LIBS "${tbx_previous_build_shared_libs}" CACHE BOOL "Build shared libraries (DLLs)." FORCE)
else ()
    unset(BUILD_SHARED_LIBS CACHE)
endif ()
unset(BUILD_SHARED_LIBS)

unset(CMAKE_FOLDER)
