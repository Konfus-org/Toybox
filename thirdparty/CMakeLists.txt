# Every third-party dependency in one place: always-on libraries first, then whatever the
# selected backends (tbx_backend() runs before this directory) pull in. Targets are global;
# the only value the parent needs back is TBX_STEAMAUDIO_ROOT (runtime dll copies).

# The cache is segmented per generator: FetchContent's sub-build trees are tied to the
# generator that created them, and CMake hard-errors when a consumer with a different
# generator reuses them.
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
    string(MAKE_C_IDENTIFIER "${CMAKE_GENERATOR}" TBX_FETCHCONTENT_GENERATOR_DIR)
    set(FETCHCONTENT_BASE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/_deps/${TBX_FETCHCONTENT_GENERATOR_DIR}" CACHE PATH
        "Location for FetchContent-vendored third-party sources")
endif()
include(FetchContent)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0)
FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0)

# Luau's CMakeLists declares an old cmake_minimum_required; let CMake accept it.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
set(LUAU_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUAU_BUILD_WEB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG 0.653)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3)
FetchContent_MakeAvailable(glm spdlog nlohmann_json entt luau assimp)

if(TBX_PLATFORM_BACKEND STREQUAL "sdl")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.6)
    FetchContent_MakeAvailable(SDL)
endif()

if(TBX_GFX_BACKEND STREQUAL "gl")
    add_subdirectory(glad)
endif()

if(TBX_UI_BACKEND STREQUAL "rmlui")
    # Text needs a font engine; "none" keeps the dependency footprint zero until fonts land.
    set(RMLUI_FONT_ENGINE "none" CACHE STRING "" FORCE)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG 6.0)
    FetchContent_MakeAvailable(rmlui)
endif()

if(TBX_AUDIO_BACKEND STREQUAL "steamaudio")
    # Steam Audio ships as a prebuilt SDK (phonon.dll); FetchContent just unpacks the release.
    FetchContent_Declare(steamaudio
        URL https://github.com/ValveSoftware/steam-audio/releases/download/v4.5.3/steamaudio_4.5.3.zip)
    FetchContent_MakeAvailable(steamaudio)
    set(TBX_STEAMAUDIO_ROOT "${steamaudio_SOURCE_DIR}")
    if(NOT EXISTS "${TBX_STEAMAUDIO_ROOT}/include/phonon.h"
       AND EXISTS "${TBX_STEAMAUDIO_ROOT}/steamaudio/include/phonon.h")
        set(TBX_STEAMAUDIO_ROOT "${TBX_STEAMAUDIO_ROOT}/steamaudio")
    endif()
    set(TBX_STEAMAUDIO_ROOT "${TBX_STEAMAUDIO_ROOT}" PARENT_SCOPE)
    add_library(phonon SHARED IMPORTED GLOBAL)
    set_target_properties(phonon PROPERTIES
        IMPORTED_LOCATION "${TBX_STEAMAUDIO_ROOT}/lib/windows-x64/phonon.dll"
        IMPORTED_IMPLIB "${TBX_STEAMAUDIO_ROOT}/lib/windows-x64/phonon.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${TBX_STEAMAUDIO_ROOT}/include")
endif()

if(TBX_PHYSICS_BACKEND STREQUAL "jolt")
    # Jolt is static (its LTO archive is incompatible with shared linking) with GPU compute
    # backends off so the runtime needs no extra SDK DLLs. Its CMakeLists lives in Build/.
    set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
    set(JPH_USE_DXC OFF CACHE BOOL "" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
    set(JPH_USE_CPU_COMPUTE ON CACHE BOOL "" FORCE)
    set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
    set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
    # Jolt bakes /Wall /WX into CMAKE_CXX_FLAGS when ON, out of reach of the warnings sweep.
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.2.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
        SYSTEM)
    FetchContent_MakeAvailable(JoltPhysics)
endif()
