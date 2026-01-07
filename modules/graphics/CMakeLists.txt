tbx_add_module(Graphics)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(Graphics PRIVATE ${SRCS} ${INCL}  "include/tbx/graphics/render_surface.h")
target_precompile_headers(Graphics PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")


target_include_directories(Graphics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Graphics
    PUBLIC
        Tbx::Debugging
        glm
)

add_library(Tbx::Graphics ALIAS Graphics)
