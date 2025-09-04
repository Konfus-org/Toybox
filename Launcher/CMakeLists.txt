add_library(Launcher)

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*")
target_sources(Launcher PRIVATE ${SRCS} ${INCL})

# Expose include dirs to dependents (build & install time)
target_include_directories(Launcher
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:Include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++ standard like cppdialect "C++20"
set_target_properties(Launcher PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Links
target_link_libraries(Launcher PUBLIC
  Tbx::Engine
)

# Propagate these defines to dependents
target_compile_definitions(Launcher PUBLIC
  GLM_FORCE_LEFT_HANDED
  GLM_DEPTH_ZERO_TO_ONE
)

# Namespaced alias for consumers
add_library(Tbx::Launcher ALIAS Launcher)
