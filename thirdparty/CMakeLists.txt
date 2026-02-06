set(CMAKE_FOLDER "third party")

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(IMGUI_PLATFORM SDL3 CACHE STRING "" FORCE)
set(IMGUI_RENDERER OpenGL3 CACHE STRING "" FORCE)
set(IMGUI_GL_LOADER GLAD CACHE STRING "" FORCE)
set(IMGUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)

function(tbx_add_thirdparty_library thirdparty_subdirectory)
    add_subdirectory("${thirdparty_subdirectory}")
    foreach(thirdparty_target IN LISTS ARGN)
        if(TARGET ${thirdparty_target})
            target_compile_options(${thirdparty_target} INTERFACE
                    $<$<CXX_COMPILER_ID:MSVC>:/W0>
                    $<$<C_COMPILER_ID:MSVC>:/W0>
                    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
                    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
                )
        endif()
    endforeach()
endfunction()

tbx_add_thirdparty_library(assimp assimp)
tbx_add_thirdparty_library(entt EnTT)
tbx_add_thirdparty_library(glad glad)
tbx_add_thirdparty_library(stbimg stbimg)
tbx_add_thirdparty_library(glm glm glm-header-only)
tbx_add_thirdparty_library(nlohmann_json nlohmann_json)
tbx_add_thirdparty_library(spdlog spdlog spdlog_header_only)
tbx_add_thirdparty_library(SDL SDL3-static SDL3-shared SDL3_test)
tbx_add_thirdparty_library(imgui imgui)

unset(CMAKE_FOLDER)
