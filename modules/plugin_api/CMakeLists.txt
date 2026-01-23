tbx_add_module(PluginApi)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
target_sources(PluginApi PRIVATE ${SRCS} ${INCL})

target_compile_definitions(PluginApi
    PUBLIC
        TBX_PLUGIN_ABI_VERSION=${TBX_PLUGIN_ABI_VERSION}
)

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
        Tbx::Debugging
        Tbx::Files
    PRIVATE
        nlohmann_json
)

add_library(Tbx::PluginApi ALIAS PluginApi)
