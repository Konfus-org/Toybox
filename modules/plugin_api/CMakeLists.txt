tbx_add_module(PluginApi)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(PluginApi PRIVATE ${SRCS} ${INCL})

target_include_directories(PluginApi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(PluginApi
    PUBLIC
        Tbx::Messaging
        Tbx::FileSystem
        Tbx::Debugging
    PRIVATE
        nlohmann_json
)

add_library(Tbx::PluginApi ALIAS PluginApi)
