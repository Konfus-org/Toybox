add_library(PluginApi STATIC)

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
        ${CMAKE_SOURCE_DIR}/modules/app/include
)

target_link_libraries(PluginApi
    PUBLIC
        Tbx::Messaging
        Tbx::Std
        Tbx::Os
    PRIVATE
        nlohmann_json
)

target_compile_definitions(PluginApi PRIVATE TBX_EXPORTING_SYMBOLS=1)

tbx_apply_common_definitions(PluginApi)

add_library(Tbx::PluginApi ALIAS PluginApi)
