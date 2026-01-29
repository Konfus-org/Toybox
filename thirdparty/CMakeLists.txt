set(CMAKE_FOLDER "third party")

# Silence warnings from third-party libraries to keep engine builds clean.
if (MSVC)
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()

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

# Configure Assimp build options for the engine.
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)

add_subdirectory(assimp)
add_subdirectory(entt)
add_subdirectory(glad)
add_subdirectory(stbimg)
add_subdirectory(glm)
add_subdirectory(nlohmann_json)
add_subdirectory(googletest)
add_subdirectory(spdlog)
add_subdirectory(SDL)
add_subdirectory(imgui)

unset(CMAKE_FOLDER)
