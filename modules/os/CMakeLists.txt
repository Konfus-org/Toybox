add_library(Os STATIC)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Os PRIVATE ${SRCS} ${INCL})

target_include_directories(Os
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Os
    PUBLIC
        Tbx::Messaging
        Tbx::Debugging
        Tbx::Std
)

target_compile_definitions(Os PRIVATE TBX_EXPORTING_SYMBOLS=1)

tbx_apply_common_definitions(Os)

add_library(Tbx::Os ALIAS Os)
