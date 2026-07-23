cmake_minimum_required(VERSION 3.28.3)
project(Toybox2 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Tooling (clangd, analyzers, the future script editor) reads compile_commands.json from the
# build tree — always export it, no matter what configured the build (.clangd points at it).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Embedded debug info (/Z7) lets MSVC compile in parallel without serializing on the PDB
# server; the linker still produces one pdb per binary.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

option(TBX_BUILD_TESTS "Build engine tests" OFF)
option(TBX_BUILD_SAMPLES "Build the sample games" ON)
option(TBX_BUILD_SHARED "Build the engine as a shared library (dll)" OFF)
option(TBX_SANITIZE "Compile engine code (not third-party) with ASan+UBSan" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(tbx_backend)
include(tbx_codegen)
include(third_party_warnings)

# --- Backend selection (see cmake/tbx_backend.cmake) ---
tbx_backend(LOGGING spdlog debug)
tbx_backend(MATH glm math)
tbx_backend(SERIALIZATION nlohmann)
tbx_backend(ECS entt)
tbx_backend(PLATFORM sdl)
tbx_backend(GPU gl gfx)
tbx_backend(PHYSICS jolt)
tbx_backend(AUDIO steamaudio)
tbx_backend(UI rmlui)
tbx_backend_list(SCRIPTING luau)

# --- Attribute codegen (see cmake/tbx_codegen.cmake): generates reflection + serialization
# registration from [[tbx::serializable]] annotations into ${CMAKE_BINARY_DIR}/generated. ---
tbx_generate_reflection()

# --- Third-party dependencies (thirdparty/CMakeLists.txt owns every FetchContent) ---
add_subdirectory(thirdparty)
if(TBX_PHYSICS_BACKEND STREQUAL "jolt" AND NOT TARGET Jolt::Jolt AND TARGET Jolt)
    add_library(Jolt::Jolt ALIAS Jolt)
endif()

# --- The engine: one library, static by default, shared with -DTBX_BUILD_SHARED=ON ---
# Public headers live in engine/include/tbx/<module>/, implementation in
# engine/src/<module>/ (backend private headers may sit next to their .cpp). Public symbols
# wear TBX_DLL_EXPORT (tbx/core/api.h): dllexport building the dll, dllimport consuming it.
file(GLOB_RECURSE TBX_SOURCES CONFIGURE_DEPENDS
    "engine/src/*.cpp" "engine/src/*.h" "engine/include/*.h")
# Backend folders are chosen by tbx_backend(); strip every backend subfolder from
# the glob, then add back only the selected ones.
list(FILTER TBX_SOURCES EXCLUDE REGEX "engine/src/(platform|gfx|physics|scripting|math|debug|serialization|audio|ui|ecs)/[^/]+/")
list(APPEND TBX_SOURCES ${TBX_PLATFORM_BACKEND_SOURCES} ${TBX_GPU_BACKEND_SOURCES} ${TBX_PHYSICS_BACKEND_SOURCES} ${TBX_SCRIPTING_BACKEND_SOURCES} ${TBX_MATH_BACKEND_SOURCES} ${TBX_SERIALIZATION_BACKEND_SOURCES} ${TBX_ECS_BACKEND_SOURCES} ${TBX_LOGGING_BACKEND_SOURCES} ${TBX_AUDIO_BACKEND_SOURCES} ${TBX_UI_BACKEND_SOURCES})
# Codegen-produced registration (generated into the build tree by tbx_codegen).
list(APPEND TBX_SOURCES ${TBX_GENERATED_SOURCES})

if(TBX_BUILD_SHARED)
    add_library(tbx SHARED ${TBX_SOURCES})
    target_compile_definitions(tbx PRIVATE TBX_EXPORTS PUBLIC TBX_SHARED)
else()
    add_library(tbx STATIC ${TBX_SOURCES})
endif()
target_include_directories(tbx PUBLIC engine/include)
# Generated registration headers (tbx/reflection.generated.h) live in the build tree.
target_include_directories(tbx PRIVATE "${TBX_GENERATED_DIR}")
# src/ on the include path so internal headers (runtime_state.h, *_internal.h) resolve from any TU —
# these define the engine state kept out of the public include/ tree (see CodeStandards.md).
target_include_directories(tbx PRIVATE engine/src)
# Generation must finish before any tbx TU compiles (reflection.cpp/serializers.cpp include it).
add_dependencies(tbx tbx_codegen)
# Header-only backends contribute an include dir instead of sources.
target_include_directories(tbx PUBLIC
    "engine/src/math/${TBX_MATH_BACKEND}"
    "engine/src/serialization/${TBX_SERIALIZATION_BACKEND}"
    "engine/src/ecs/${TBX_ECS_BACKEND}")
target_include_directories(tbx PRIVATE thirdparty/stbimg/include)
# Precompile the heavyweights every TU re-parses otherwise (std, math/serialization backend
# headers, entt) — the single biggest incremental-build lever.
target_precompile_headers(tbx PRIVATE engine/src/pch.h)
# glm/nlohmann/entt types ARE the swappable public seams (tbx_math_backend.h,
# tbx_serialization_backend.h, tbx_ecs_backend.h); every other third-party lib is private.
target_link_libraries(tbx
    PUBLIC glm::glm nlohmann_json::nlohmann_json EnTT::EnTT
    PRIVATE spdlog::spdlog assimp::assimp
)
# Sanitizers cover ENGINE code only: PUBLIC propagates to everything linking tbx (tests,
# samples, tbx_run) while vendored libraries build uninstrumented — RmlUi 6.0 in particular
# has known benign UB (its NonCopyMoveable empty base) that would abort a run-to-first-error
# UBSan. Mixed instrumentation links fine.
if(TBX_SANITIZE)
    target_compile_options(tbx PUBLIC -fsanitize=address,undefined -fno-sanitize-recover=undefined)
    target_link_options(tbx PUBLIC -fsanitize=address,undefined)
endif()
if("luau" IN_LIST TBX_SCRIPTING_BACKENDS)
    target_link_libraries(tbx PRIVATE Luau.Compiler Luau.VM)
endif()
if(TBX_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(tbx PRIVATE Jolt::Jolt)
endif()
if(TBX_AUDIO_BACKEND STREQUAL "steamaudio")
    target_link_libraries(tbx PRIVATE phonon)
endif()
if(TBX_UI_BACKEND STREQUAL "rmlui")
    target_link_libraries(tbx PRIVATE RmlUi::Core)
endif()

if(TBX_PLATFORM_BACKEND STREQUAL "sdl")
    target_link_libraries(tbx PRIVATE SDL3::SDL3-static)
endif()
if(TBX_GPU_BACKEND STREQUAL "gl")
    target_link_libraries(tbx PRIVATE glad)
endif()

# Runtime DLLs (phonon) ride along next to every executable.
function(tbx_copy_runtime_dlls target)
    if(TBX_AUDIO_BACKEND STREQUAL "steamaudio")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:phonon>" "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()

# --- Executables ---
if(TBX_BUILD_SAMPLES AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/samples")
    add_subdirectory(samples)
endif()
if(TBX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(engine/tests)
endif()

# Third-party code is not ours to fix: silence its warnings after the whole tree is configured.
tbx_silence_third_party_warnings("${CMAKE_SOURCE_DIR}")
