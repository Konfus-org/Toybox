tbx_add_module(Assets)

file(GLOB_RECURSE ASSET_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE ASSET_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources(Assets PRIVATE ${ASSET_SOURCES} ${ASSET_HEADERS} )

target_include_directories(Assets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Assets
    PUBLIC
        Tbx::Common
        Tbx::Messaging
)

add_library(Tbx::Assets ALIAS Assets)
