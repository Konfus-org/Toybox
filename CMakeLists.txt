cmake_minimum_required(VERSION 3.28.3)
project(Toybox2 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TBX_BUILD_TESTS "Build engine tests" OFF)
option(TBX_BUILD_SAMPLE "Build the sample game" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(tbx_backend)
include(third_party_warnings)

# --- Third-party dependencies (FetchContent, cached under thirdparty/_deps) ---
# The cache is segmented per generator: FetchContent's sub-build trees are tied to the
# generator that created them, and CMake hard-errors when a consumer with a different
# generator reuses them.
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
    string(MAKE_C_IDENTIFIER "${CMAKE_GENERATOR}" TBX_FETCHCONTENT_GENERATOR_DIR)
    set(FETCHCONTENT_BASE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/_deps/${TBX_FETCHCONTENT_GENERATOR_DIR}" CACHE PATH
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
FetchContent_MakeAvailable(glm spdlog nlohmann_json entt luau)

# --- Backend selection (see cmake/tbx_backend.cmake) ---
tbx_backend(MATH glm)
tbx_backend(SERIALIZATION nlohmann)
tbx_backend(PLATFORM sdl)
tbx_backend(GFX gl)
tbx_backend(PHYSICS jolt)
tbx_backend_list(SCRIPTING luau)

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
    add_subdirectory(thirdparty/glad)
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
    if(NOT TARGET Jolt::Jolt AND TARGET Jolt)
        add_library(Jolt::Jolt ALIAS Jolt)
    endif()
endif()

# --- The engine: one static library ---
# Public headers live in engine/include/tbx/<module>/, implementation in
# engine/src/<module>/ (backend private headers may sit next to their .cpp).
file(GLOB_RECURSE TBX_SOURCES CONFIGURE_DEPENDS
    "engine/src/*.cpp" "engine/src/*.h" "engine/include/*.h")
# Backend folders are chosen by tbx_backend(); strip every backend subfolder from
# the glob, then add back only the selected ones.
list(FILTER TBX_SOURCES EXCLUDE REGEX "engine/src/(platform|gfx|physics|scripting|math|serialization)/[^/]+/")
list(APPEND TBX_SOURCES ${TBX_PLATFORM_BACKEND_SOURCES} ${TBX_GFX_BACKEND_SOURCES} ${TBX_PHYSICS_BACKEND_SOURCES} ${TBX_SCRIPTING_BACKEND_SOURCES})

add_library(tbx STATIC ${TBX_SOURCES})
target_include_directories(tbx PUBLIC engine/include)
# Header-only backends contribute an include dir instead of sources.
target_include_directories(tbx PUBLIC
    "engine/src/math/${TBX_MATH_BACKEND}"
    "engine/src/serialization/${TBX_SERIALIZATION_BACKEND}")
target_include_directories(tbx PRIVATE thirdparty/stbimg/include)
target_link_libraries(tbx
    PUBLIC glm::glm spdlog::spdlog nlohmann_json::nlohmann_json EnTT::EnTT
)
if("luau" IN_LIST TBX_SCRIPTING_BACKENDS)
    target_link_libraries(tbx PRIVATE Luau.Compiler Luau.VM)
endif()
if(TBX_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(tbx PRIVATE Jolt::Jolt)
endif()

if(TBX_PLATFORM_BACKEND STREQUAL "sdl")
    target_link_libraries(tbx PRIVATE SDL3::SDL3-static)
endif()
if(TBX_GFX_BACKEND STREQUAL "gl")
    target_link_libraries(tbx PRIVATE glad)
endif()

# --- Executables ---
add_subdirectory(runtime)
if(TBX_BUILD_SAMPLE AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/sample")
    add_subdirectory(sample)
endif()
if(TBX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(engine/tests)
endif()

# Third-party code is not ours to fix: silence its warnings after the whole tree is configured.
tbx_silence_third_party_warnings("${CMAKE_SOURCE_DIR}")
