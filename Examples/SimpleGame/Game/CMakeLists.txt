add_library(SimpleGame SHARED)

# Sources
file(GLOB_RECURSE INCL   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h*")
file(GLOB_RECURSE SRCS   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/*.*")
file(GLOB_RECURSE META CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")
target_sources(SimpleGame PRIVATE ${SRCS} ${INCL} ${ASSETS} ${META})
source_group("Assets" FILES ${ASSETS})

# Expose include dirs to dependents (build & install time)
target_include_directories(SimpleGame
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Assets>
    $<INSTALL_INTERFACE:Include>
    $<INSTALL_INTERFACE:Assets>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# C++20
set_target_properties(Engine PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Links
target_link_libraries(SimpleGame PRIVATE
  Tbx::Engine

)

# Define plugins to use
target_link_libraries(SimpleGame PRIVATE
  Tbx::Plugin::ImGuiDebugUI
  Tbx::Plugin::JIMSAssetLoader
  Tbx::Plugin::OpenGLRendering
  Tbx::Plugin::SDL3Input
  Tbx::Plugin::SDL3Windowing
  Tbx::Plugin::SpdLogging
)

# Copy assets
add_custom_command(TARGET SimpleGame POST_BUILD
  COMMENT "Copying assets to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:SimpleGame>/Assets"
  VERBATIM
)

# Copy meta file since we will treat our game as a plugin
add_custom_command(TARGET SimpleGame POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/SimpleGame.meta"
          "$<TARGET_FILE_DIR:SimpleGame>/SimpleGame.meta")

# Nice alias for consumers
add_library(Tbx::Examples::SimpleGame ALIAS SimpleGame)