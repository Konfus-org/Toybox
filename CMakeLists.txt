cmake_minimum_required(VERSION 3.5...4.0)
project(Toybox LANGUAGES CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the output folder where your program will be created
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Build/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Build/bin)

# Enable testing
enable_testing()

# Global configs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ReleaseAssert" CACHE STRING "" FORCE)

target_compile_definitions(Toybox
    PRIVATE
        $<$<CONFIG:Debug>:TBX_DEBUG>
        $<$<CONFIG:Release>:TBX_RELEASE>
        $<$<CONFIG:ReleaseAssert>:TBX_RELEASE_ASSERT>
)

if(MSVC)
    # For MSVC, explicitly set the runtime library to dynamic variants
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_compile_options(/MP)  # Enable multi-processor compile
else()
  # For other compilers (e.g., GCC, Clang), set BUILD_SHARED_LIBS to ON
  # This makes 'add_library()' default to shared libraries
  set(BUILD_SHARED_LIBS ON)
  # You might also need to ensure executables link dynamically
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed") # Example for Linux
endif()

# Add core projects
add_subdirectory(Engine)

# Add dependencies:
add_subdirectory(./Dependencies/glad)
add_subdirectory(./Dependencies/glm)
add_subdirectory(./Dependencies/googletest)
add_subdirectory(./Dependencies/ImGui)
add_subdirectory(./Dependencies/nlohmann_json)
add_subdirectory(./Dependencies/SDL)
add_subdirectory(./Dependencies/spdlog)
add_subdirectory(./Dependencies/stbimg)
add_subdirectory(./Dependencies/sys_info)

# Add plugins:
#add_subdirectory(./Plugins)

if(MSVC)
    # Put Toybox projects into "Toybox" folder
    set_property(TARGET Engine PROPERTY FOLDER "Toybox")
    
    # Put dependencies into "Dependencies" folder
    set_property(TARGET glad PROPERTY FOLDER "Dependencies")
    set_property(TARGET glm PROPERTY FOLDER "Dependencies")
    set_property(TARGET googletest PROPERTY FOLDER "Dependencies")
    set_property(TARGET ImGui PROPERTY FOLDER "Dependencies")
    set_property(TARGET nlohmann_json PROPERTY FOLDER "Dependencies")
    set_property(TARGET SDL PROPERTY FOLDER "Dependencies")
    set_property(TARGET spdlog PROPERTY FOLDER "Dependencies")
    set_property(TARGET stbimg PROPERTY FOLDER "Dependencies")
    set_property(TARGET sys_info PROPERTY FOLDER "Dependencies")

    # Put ALL_BUILD and ZERO_CHECK into "CMakeTargets" folder
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
endif()