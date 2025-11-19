set(CMAKE_FOLDER "3rd Party")

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

include(FetchContent)

add_subdirectory(glad)
add_subdirectory(stbimg)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/glm/CMakeLists.txt")
  add_subdirectory(glm)
else()
  message(STATUS "Fetching glm because the submodule is unavailable")
  FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
    URL_HASH SHA256=09c5716296787e1f7fcb87b1cbdbf26814ec1288ed6259ccd30d5d9795809fa5
  )
  FetchContent_MakeAvailable(glm)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/CMakeLists.txt")
  add_subdirectory(nlohmann_json)
else()
  message(STATUS "Fetching nlohmann_json because the submodule is unavailable")
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.zip
    URL_HASH SHA256=95651d7d1fcf2e5c3163c3d37df6d6b3e9e5027299e6bd050d157322ceda9ac9
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/googletest/CMakeLists.txt")
  add_subdirectory(googletest)
else()
  message(STATUS "Fetching googletest because the submodule is unavailable")
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    URL_HASH SHA256=1f357c27ca988c3f7c6b4bf68a9395005ac6761f034046e9dde0896e3aba00e4
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/CMakeLists.txt")
  add_subdirectory(spdlog)
else()
  message(WARNING "Skipping optional dependency 'spdlog' because it is not available")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt")
  add_subdirectory(SDL)
else()
  message(WARNING "Skipping optional dependency 'SDL' because it is not available")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/CMakeLists.txt")
  add_subdirectory(imgui)
else()
  message(WARNING "Skipping optional dependency 'imgui' because it is not available")
endif()
