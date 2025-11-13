add_library(Time STATIC)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Time PRIVATE ${SRCS} ${INCL})

target_include_directories(Time
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(TBX_BUILD_SHARED)
    target_compile_definitions(Time PRIVATE TBX_SHARED_LIB=1)
endif()

target_compile_definitions(Time PRIVATE TBX_EXPORTING_SYMBOLS=1)

target_link_libraries(Time
    PUBLIC
        Tbx::Std
)

tbx_apply_common_definitions(Time)

add_library(Tbx::Time ALIAS Time)
