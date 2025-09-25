add_library(Engine SHARED)

# PCH
target_precompile_headers(Engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Include/Tbx/PCH.h")

# Sources
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*")
target_sources(Engine PRIVATE ${SRCS} ${INCL})

# Expose include dirs to dependents (build & install time)
target_include_directories(Engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:Include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# C++20
set_target_properties(Engine PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

# Link deps (prefer exported targets)
target_link_libraries(Engine PRIVATE
    glm
    nlohmann_json
)

if(WIN32)
  target_link_libraries(Engine PUBLIC DbgHelp)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(Engine PRIVATE dl)
endif()

# Propagate these defines to dependents
target_compile_definitions(Engine PUBLIC
  GLM_ENABLE_EXPERIMENTAL
  GLM_FORCE_LEFT_HANDED
  GLM_DEPTH_ZERO_TO_ONE
  TBX_WORKING_ROOT_DIR="$<TARGET_FILE_DIR:Engine>"
)

target_compile_definitions(Engine PRIVATE
  COMPILING_TOYBOX=1
)

# Namespaced alias for consumers
add_library(Tbx::Engine ALIAS Engine)
