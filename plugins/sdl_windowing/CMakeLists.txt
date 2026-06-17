# This plugin owns SDL3 (shared) and the in-tree stbimg (used to decode the window icon,
# since upstream SDL has no multi-format image loader). EnTT is engine-core, owned by
# Tbx::Engine. The `if(NOT TARGET ...)` guards make shared/local deps idempotent.
include(FetchContent)
if(NOT TARGET SDL3::SDL3-shared)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(SDL_WAYLAND ON CACHE BOOL "" FORCE)
        set(SDL_X11 ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.6
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(SDL)
    set_target_properties(SDL3-shared PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
endif()

if(NOT TARGET stbimg)
    add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/stbimg" "${CMAKE_BINARY_DIR}/thirdparty/stbimg")
endif()

tbx_add_plugin(
    NAME SdlWindowing
    PRIVATE_LINKS
        EnTT::EnTT
        Tbx::Plugins::SdlBaseSystems
        SDL3::SDL3-shared
        stbimg
)
