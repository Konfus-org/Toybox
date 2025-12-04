add_library(SDL3Audio SHARED)

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*")
file(GLOB_RECURSE META CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")
target_sources(SDL3Audio PRIVATE ${SRCS} ${INCL} ${META})

# Expose include dirs to dependents (build & install time)
target_include_directories(SDL3Audio
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:Include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++20
set_target_properties(SDL3Audio PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link libs
target_link_libraries(SDL3Audio PRIVATE
  Tbx::Engine
  SDL3-shared
)

# Copy plugin meta file to build dir
add_custom_command(TARGET SDL3Audio POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/SDL3Audio.meta"
          "$<TARGET_FILE_DIR:SDL3Audio>/SDL3Audio.meta")

# Namespaced alias for consumers
add_library(Tbx::Plugin::SDL3Audio ALIAS SDL3Audio)
