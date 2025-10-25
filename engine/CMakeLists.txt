if (CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "RTTI must be enabled for Toybox")
endif()

# Options
option(TBX_PLUGINS_DIR "Plugin directory" "")
option(TBX_ASSETS_DIR "Asset directory" "Assets")
option(TBX_BUILD_SHARED "Build as a shared library" ON)
option(TBX_VERB_LOGGING "Enable verbose logging" OFF)

# Library type
if(TBX_BUILD_SHARED)
  add_library(Engine SHARED)
else()
  add_library(Engine STATIC)
endif()

# PCH (optional)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/PCH.h")
  target_precompile_headers(Engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/PCH.h")
endif()

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(Engine PRIVATE ${SRCS} ${INCL})

# Expose include dirs to dependents (build & install time)
target_include_directories(Engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++ standard (inherits 23 from root, but set explicitly here for standalone builds)
set_target_properties(Engine PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link deps
target_link_libraries(Engine PRIVATE
    glm
    nlohmann_json
)
if(WIN32)
  target_link_libraries(Engine PUBLIC DbgHelp)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(Engine PRIVATE dl)
endif()

# Defines
target_compile_definitions(Engine PUBLIC
  # Glm
  GLM_ENABLE_EXPERIMENTAL=1
  GLM_FORCE_LEFT_HANDED=1
  GLM_DEPTH_ZERO_TO_ONE=1

  # Dirs
  TBX_WORKING_ROOT_DIR="$<TARGET_FILE_DIR:Engine>"
  TBX_ASSETS_DIR="${TBX_ASSETS_DIR}"
  TBX_PLUGINS_DIR="${TBX_PLUGINS_DIR}"

  # Options
  $<$<BOOL:${TBX_BUILD_SHARED}>:TBX_SHARED_LIB=1>
  $<$<BOOL:${TBX_VERB_LOGGING}>:TBX_VERBOSE_LOGGING=1>

  # Platform
  $<$<PLATFORM_ID:Windows>:TBX_PLATFORM_WINDOWS=1;WIN32_LEAN_AND_MEAN=1;NOMINMAX=1;_CRT_SECURE_NO_WARNINGS=1>
  $<$<PLATFORM_ID:Darwin>:TBX_PLATFORM_MACOS=1>
  $<$<PLATFORM_ID:Linux>:TBX_PLATFORM_LINUX=1>

  # Build
  $<$<CONFIG:Debug>:TBX_DEBUG=1;TBX_ASSERTS_ENABLED=1>
  $<$<CONFIG:Release>:TBX_RELEASE=1>
  $<$<CONFIG:ReleaseAssert>:TBX_RELEASE=1;TBX_ASSERTS_ENABLED=1>
)
target_compile_definitions(Engine PRIVATE
  TBX_EXPORTING_SYMBOLS=1
)
