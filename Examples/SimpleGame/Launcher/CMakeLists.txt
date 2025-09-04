add_executable(SGLauncher)

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp*")
target_sources(SGLauncher PRIVATE ${INCL} ${SRCS})

# C++20
set_target_properties(SGLauncher PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Links
target_link_libraries(SGLauncher PRIVATE
  Tbx::Examples::SimpleGame
  Tbx::Engine
  Tbx::Launcher
)

# Nice alias for consumers
add_executable(Tbx::Examples::SGLauncher ALIAS SGLauncher)