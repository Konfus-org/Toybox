# This plugin owns Jolt (CPU physics). EnTT and glm are engine-core dependencies owned by
# Tbx::Engine; this plugin only links them. Jolt is built static (its LTO archive format is
# incompatible with being linked into a shared plugin), and its GPU compute backends are
# disabled so the runtime needs no extra SDK DLLs (e.g. DXC).
include(FetchContent)
if(NOT TARGET Jolt::Jolt)
    set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
    set(JPH_USE_DXC OFF CACHE BOOL "" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_CPU_COMPUTE ON CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    # Jolt bakes -Wall -Werror (or /Wall /WX) into CMAKE_CXX_FLAGS when this is ON, out of
    # reach of the third_party_warnings sweep, which only edits properties.
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)

    set(_jolt_had_shared FALSE)
    if(DEFINED BUILD_SHARED_LIBS)
        set(_jolt_had_shared TRUE)
        set(_jolt_prev_shared "${BUILD_SHARED_LIBS}")
    endif()
    set(BUILD_SHARED_LIBS OFF)

    # Jolt's CMakeLists lives in the Build/ subdirectory of the repo.
    FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.2.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
        SYSTEM)
    FetchContent_MakeAvailable(JoltPhysics)

    if(_jolt_had_shared)
        set(BUILD_SHARED_LIBS "${_jolt_prev_shared}")
    else()
        unset(BUILD_SHARED_LIBS)
    endif()

    if(NOT TARGET Jolt::Jolt AND TARGET Jolt)
        add_library(Jolt::Jolt ALIAS Jolt)
    endif()
    # Jolt force-defines _DEBUG in Debug builds (PUBLIC, so it reaches this plugin's objects
    # too). When a preset pins a non-debug CRT (the sanitize presets use MultiThreadedDLL
    # because ASan does not support the debug CRT), _DEBUG makes the MS STL emit debug-CRT
    # references (msvcprtd, _malloc_dbg) that the release-CRT link cannot resolve. Strip the
    # define so the CRT selection alone decides _DEBUG.
    if(WIN32 AND DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Debug")
        foreach(_jolt_defs_prop COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
            get_target_property(_jolt_defs Jolt ${_jolt_defs_prop})
            if(_jolt_defs)
                list(REMOVE_ITEM _jolt_defs "$<$<CONFIG:Debug>:_DEBUG>")
                set_target_properties(Jolt PROPERTIES ${_jolt_defs_prop} "${_jolt_defs}")
            endif()
        endforeach()
    endif()
endif()

tbx_add_plugin(
    NAME JoltPhysics
    PRIVATE_LINKS
        Jolt::Jolt
        EnTT::EnTT
        glm
)
