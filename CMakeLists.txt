cmake_minimum_required(VERSION 3.20)
project(Toybox LANGUAGES C CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/lib")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/pdb")

# Enable testing
enable_testing()

# Global configs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ReleaseAssert" CACHE STRING "" FORCE)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/MP)
else()
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
endif()

# Provide flag variables for the custom "ReleaseAssert" configuration.
foreach(_var
    CMAKE_C_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_EXE_LINKER_FLAGS_RELEASE
    CMAKE_SHARED_LINKER_FLAGS_RELEASE
    CMAKE_STATIC_LINKER_FLAGS_RELEASE
    CMAKE_MODULE_LINKER_FLAGS_RELEASE)
  string(REPLACE "_RELEASE" "_RELEASEASSERT" _var_name "${_var}")
  set(${_var_name} "${${_var}}" CACHE STRING "" FORCE)
endforeach()

# Dependencies
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(IMGUI_PLATFORM SDL3 CACHE STRING "" FORCE) 
set(IMGUI_RENDERER OpenGL3 CACHE STRING "" FORCE)
set(IMGUI_GL_LOADER GLAD CACHE STRING "" FORCE)
set(IMGUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(Dependencies/glad)
add_subdirectory(Dependencies/glm)
add_subdirectory(Dependencies/googletest)
add_subdirectory(Dependencies/nlohmann_json)
add_subdirectory(Dependencies/SDL)
add_subdirectory(Dependencies/spdlog)
add_subdirectory(Dependencies/stbimg)
add_subdirectory(Dependencies/imgui)
set_property(TARGET imgui PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET glad PROPERTY POSITION_INDEPENDENT_CODE ON)

# Core projects
add_subdirectory(Engine)
add_subdirectory(Engine/Tests)
add_subdirectory(Launcher)
target_link_libraries(Engine)
target_link_libraries(Tests)
target_link_libraries(Launcher)

# Examples
add_subdirectory(Examples/3dDemo/Runtime)
add_subdirectory(Examples/3dDemo/Launcher)

# Plugins:
add_subdirectory(Plugins/Toybox-ImGui-DebugUI-Plugin)
add_subdirectory(Plugins/Toybox-JIMS-AssetLoader-Plugin)
add_subdirectory(Plugins/Toybox-OpenGL-Rendering-Plugin)
add_subdirectory(Plugins/Toybox-SDL-Plugins)
add_subdirectory(Plugins/Toybox-Spd-Logging-Plugin)

# Organize everything into folders if using VS
if(MSVC)
    # Core
    set_property(TARGET Engine PROPERTY FOLDER "Toybox")
    set_property(TARGET Tests PROPERTY FOLDER "Toybox")
    set_property(TARGET Launcher PROPERTY FOLDER "Toybox")
    
    # Plugins
    set_property(TARGET ImGuiDebugUI PROPERTY FOLDER "Toybox/Plugins")
    set_property(TARGET JIMSAssetLoader PROPERTY FOLDER "Toybox/Plugins")
    set_property(TARGET OpenGLRendering PROPERTY FOLDER "Toybox/Plugins")
    set_property(TARGET SDL3Input PROPERTY FOLDER "Toybox/Plugins")
    set_property(TARGET SDL3Windowing PROPERTY FOLDER "Toybox/Plugins")
    set_property(TARGET SDL3OpenGlGraphicsContext PROPERTY FOLDER "Toybox/Plugins")
    set_property(TARGET SpdLogging PROPERTY FOLDER "Toybox/Plugins")
    
    # Examples
    set_property(TARGET 3dDemoRuntime PROPERTY FOLDER "Toybox/Examples")
    set_property(TARGET 3dDemoLauncher PROPERTY FOLDER "Toybox/Examples")

    # Dependencies
    set_property(TARGET glad PROPERTY FOLDER "Dependencies")
    set_property(TARGET glm PROPERTY FOLDER "Dependencies")
    set_property(TARGET gtest PROPERTY FOLDER "Dependencies")
    set_property(TARGET gtest_main PROPERTY FOLDER "Dependencies")
    set_property(TARGET gmock PROPERTY FOLDER "Dependencies")
    set_property(TARGET gmock_main PROPERTY FOLDER "Dependencies")
    set_property(TARGET imgui PROPERTY FOLDER "Dependencies")
    set_property(TARGET nlohmann_json PROPERTY FOLDER "Dependencies")
    set_property(TARGET SDL_uclibc PROPERTY FOLDER "Dependencies")
    set_property(TARGET SDL3-shared PROPERTY FOLDER "Dependencies")
    set_property(TARGET spdlog PROPERTY FOLDER "Dependencies")
    set_property(TARGET stbimg PROPERTY FOLDER "Dependencies")

    # Cmake
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
    set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT 3dDemoLauncher)
endif()
