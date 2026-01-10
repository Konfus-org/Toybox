tbx_add_module(Files)

file(GLOB_RECURSE FILE_SYSTEM_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE FILE_SYSTEM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Files PRIVATE ${FILE_SYSTEM_SOURCES} ${FILE_SYSTEM_HEADERS} )
target_precompile_headers(Files PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")


target_include_directories(Files
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Files
    PUBLIC
        Tbx::Common
    PRIVATE
        nlohmann_json
)

add_library(Tbx::Files ALIAS Files)
