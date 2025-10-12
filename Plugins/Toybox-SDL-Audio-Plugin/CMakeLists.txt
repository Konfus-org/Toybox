set(LibName SDL3Audio)
add_library(${LibName} SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp")
file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
file(GLOB_RECURSE META CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.meta")

target_sources(${LibName} PRIVATE ${SRCS} ${INCL} ${META})

target_include_directories(${LibName}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Source>
    $<INSTALL_INTERFACE:Source>
)

set_target_properties(${LibName} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

if(TARGET SDL3::SDL3)
  target_link_libraries(${LibName} PRIVATE SDL3::SDL3)
elseif(TARGET SDL3-shared)
  target_link_libraries(${LibName} PRIVATE SDL3-shared)
else()
  message(FATAL_ERROR "SDL3Audio plugin requires SDL3 to be built")
endif()

target_link_libraries(${LibName} PRIVATE Tbx::Engine)

add_custom_command(TARGET ${LibName} POST_BUILD
  COMMENT "Copying plugin meta file to build dir"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/${LibName}.meta"
          "$<TARGET_FILE_DIR:${LibName}>/${LibName}.meta")

add_library(Tbx::Plugin::${LibName} ALIAS ${LibName})
