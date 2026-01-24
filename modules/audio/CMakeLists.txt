tbx_add_module(Audio)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(Audio PRIVATE ${SRCS} ${INCL})

target_include_directories(Audio
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Audio
    PUBLIC
        Tbx::Common
)

add_library(Tbx::Audio ALIAS Audio)
