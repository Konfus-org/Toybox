add_library(Messaging STATIC)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Messaging PRIVATE ${SRCS} ${INCL})

target_include_directories(Messaging
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/modules/app/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Messaging
    PUBLIC
        Tbx::Time
        Tbx::Std
        Tbx::Debugging
)

target_compile_definitions(Messaging PRIVATE TBX_EXPORTING_SYMBOLS=1)

tbx_apply_common_definitions(Messaging)

add_library(Tbx::Messaging ALIAS Messaging)
