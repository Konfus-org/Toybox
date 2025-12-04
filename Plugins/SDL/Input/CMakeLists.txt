add_library(SDL3Input SHARED)

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*")
file(GLOB_RECURSE META CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")
target_sources(SDL3Input PRIVATE ${SRCS} ${INCL} ${META})

# Expose include dirs to dependents (build & install time)
target_include_directories(SDL3Input
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:Include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++20
set_target_properties(SDL3Input PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link libs
target_link_libraries(SDL3Input PRIVATE
  Tbx::Engine
  SDL3-shared
)

# Copy plugin meta file to build dir
add_custom_command(TARGET SDL3Input POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/SDL3Input.meta"
          "$<TARGET_FILE_DIR:SDL3Input>/SDL3Input.meta")

# Namespaced alias for consumers
add_library(Tbx::Plugin::SDL3Input ALIAS SDL3Input)
