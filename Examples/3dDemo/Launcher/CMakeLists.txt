add_executable(3dDemoLauncher)

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp*")
target_sources(3dDemoLauncher PRIVATE ${INCL} ${SRCS})

# C++20
set_target_properties(3dDemoLauncher PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Links
target_link_libraries(3dDemoLauncher PRIVATE
  Tbx::Examples::3dDemoRuntime
  Tbx::Launcher
)

# Nice alias for consumers
add_executable(Tbx::Examples::3dDemoLauncher ALIAS 3dDemoLauncher)