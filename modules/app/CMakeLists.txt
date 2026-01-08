tbx_add_module(App)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(App PRIVATE ${SRCS} ${INCL})

target_precompile_headers(App PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")

target_include_directories(App
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(App
    PUBLIC
        Tbx::Time
        Tbx::Math
        Tbx::Messaging
        Tbx::PluginApi
        Tbx::ECS
        Tbx::Debugging
        Tbx::Graphics
)

add_library(Tbx::App ALIAS App)
