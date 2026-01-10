tbx_add_module(Async)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(Async PRIVATE ${SRCS} ${INCL})

target_precompile_headers(Async PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")

target_include_directories(Async
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Async
    PUBLIC
        Tbx::Common
)

add_library(Tbx::Async ALIAS Async)
