set(LibName 3dDemoRuntime)
add_library(${LibName} SHARED)

# Sources
file(GLOB_RECURSE DEPS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/*.*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.*")
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/**.h*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/**.cpp*")
file(GLOB_RECURSE META CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")
target_sources(${LibName} PRIVATE ${SRCS} ${INCL} ${DEPS} ${META} ${ASSETS})
source_group("Assets" FILES ${ASSETS})

# Expose include dirs to dependents (build & install time)
target_include_directories(${LibName}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:Include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++20
set_target_properties(${LibName} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link libs
target_link_libraries(${LibName} PRIVATE
  Tbx::Engine
  Tbx::Plugin::ImGuiImmediateUI
  Tbx::Plugin::JIMSAssetLoader
  Tbx::Plugin::OpenGLRendering
  Tbx::Plugin::SDL3OpenGlGraphicsContext
  Tbx::Plugin::SDL3Windowing
  Tbx::Plugin::SDL3Input
  Tbx::Plugin::SDL3Audio
  Tbx::Plugin::SpdLogging
)

# Copy plugin meta file to build dir
add_custom_command(TARGET ${LibName} POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/${LibName}.meta"
          "$<TARGET_FILE_DIR:${LibName}>/${LibName}.meta")

# Copy assets
add_custom_command(TARGET ${LibName} POST_BUILD
  COMMENT "Copying assets to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:${LibName}>/Assets"
  VERBATIM
)

# Namespaced alias for consumers
add_library(Tbx::Examples::${LibName} ALIAS ${LibName})