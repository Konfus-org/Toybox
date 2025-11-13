add_library(Debugging STATIC)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Debugging PRIVATE ${SRCS} ${INCL})

target_include_directories(Debugging
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Debugging
    PUBLIC
        Tbx::Messaging
        Tbx::Std
)

target_compile_definitions(Debugging PRIVATE TBX_EXPORTING_SYMBOLS=1)

tbx_apply_common_definitions(Debugging)

add_library(Tbx::Debugging ALIAS Debugging)
