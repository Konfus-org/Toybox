tbx_add_module(Assets)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(Assets PRIVATE ${SRCS} ${INCL})

target_include_directories(Assets
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Assets
    PUBLIC
        Tbx::Common
        Tbx::Messaging
        Tbx::Audio
        Tbx::Graphics
)

add_library(Tbx::Assets ALIAS Assets)
