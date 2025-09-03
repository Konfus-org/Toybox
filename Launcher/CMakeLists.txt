add_executable(Launcher)

# Sources (match your Premake globs)
file(GLOB_RECURSE LAUNCHER_SRC  CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/**/*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/**/*.hpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/**/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/**/*.md")

target_sources(Launcher PRIVATE ${LAUNCHER_SRC})

# Windows-only: .rc and .ico (mirrors your filter { 'system:windows' })
if(WIN32)
  target_sources(Launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Launcher.rc")
  file(GLOB LAUNCHER_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.ico")
  target_sources(Launcher PRIVATE ${LAUNCHER_ICONS})
endif()

# C++ standard like cppdialect "C++20"
set_target_properties(Launcher PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
)

# Include dirs
target_include_directories(Launcher PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Include"
  "${CMAKE_CURRENT_SOURCE_DIR}/Source"
  "${CMAKE_CURRENT_SOURCE_DIR}/../Engine/Include"
  ${TBX_STATIC_PLUGIN_DIRS}
)

# Links
target_link_libraries(Launcher PRIVATE
  Engine
  ${TBX_STATIC_PLUGIN_LINKS}
  ${TBX_DYNAMIC_PLUGIN_LINKS}
)
