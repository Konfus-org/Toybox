add_library(3dDemoRuntime SHARED)

# Sources
file(GLOB_RECURSE INCL   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h*")
file(GLOB_RECURSE SRCS   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.*")
file(GLOB_RECURSE META   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")
target_sources(3dDemoRuntime PRIVATE ${SRCS} ${INCL} ${ASSETS} ${META})
source_group("Assets" FILES ${ASSETS})

# Expose include dirs to dependents (build & install time)
target_include_directories(3dDemoRuntime
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Assets>
    $<INSTALL_INTERFACE:Include>
    $<INSTALL_INTERFACE:Assets>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# C++20
set_target_properties(3dDemoRuntime PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link libs
target_link_libraries(3dDemoRuntime PRIVATE
  Tbx::Engine
  Tbx::Plugin::ImGuiImmediateUI
  Tbx::Plugin::JIMSAssetLoader
  Tbx::Plugin::OpenGLRendering
  Tbx::Plugin::SDL3OpenGlGraphicsContext
  Tbx::Plugin::SDL3Windowing
  Tbx::Plugin::SDL3Input
  Tbx::Plugin::SpdLogging
)

# Copy plugin meta file to build dir
add_custom_command(TARGET 3dDemoRuntime POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/3dDemoRuntime.meta"
          "$<TARGET_FILE_DIR:3dDemoRuntime>/3dDemoRuntime.meta")

# Copy assets
add_custom_command(TARGET 3dDemoRuntime POST_BUILD
  COMMENT "Copying assets to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:3dDemoRuntime>/Assets"
  VERBATIM
)

# Namespaced alias for consumers
add_library(Tbx::Examples::3dDemoRuntime ALIAS 3dDemoRuntime)