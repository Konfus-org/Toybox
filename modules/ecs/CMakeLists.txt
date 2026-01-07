tbx_add_module(ECS)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(ECS PRIVATE ${SRCS} ${INCL})
target_precompile_headers(ECS PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")


target_include_directories(ECS
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ECS
    PUBLIC
        Tbx::Common
        Tbx::Messaging
        EnTT::EnTT
)

add_library(Tbx::ECS ALIAS ECS)