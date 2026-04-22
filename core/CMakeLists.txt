option(TBX_BUILD_SHARED "Build Toybox core as a shared library" ON)
option(TBX_VERB_LOGGING "Enable verbose logging" OFF)

set(TBX_PLUGIN_ABI_VERSION 1 CACHE STRING "Plugin ABI version used for plugin compatibility checks" FORCE)

message(STATUS "")
message(STATUS "Configuring Toybox Core:")

if(CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "Tbx: RTTI must be enabled for Toybox")
endif()

message(STATUS "Tbx Options:")
message(STATUS "   TBX_BUILD_SHARED: ${TBX_BUILD_SHARED}")
message(STATUS "   TBX_VERB_LOGGING: ${TBX_VERB_LOGGING}")

if(TBX_BUILD_SHARED)
    add_library(Core SHARED)
    target_compile_definitions(Core
        PUBLIC
            TBX_SHARED_LIB
        PRIVATE
            TBX_EXPORTING_SYMBOLS
    )
else()
    add_library(Core STATIC)
endif()

add_library(Tbx::Core ALIAS Core)

foreach(tbx_legacy_core_alias
    App
    Assets
    Async
    Audio
    Common
    Debugging
    ECS
    Files
    Graphics
    Input
    Math
    Messaging
    Physics
    PluginApi
    Time
)
    add_library(Tbx::${tbx_legacy_core_alias} ALIAS Core)
endforeach()

file(GLOB_RECURSE TBX_CORE_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE TBX_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Core PRIVATE ${TBX_CORE_SOURCES} ${TBX_CORE_INCLUDES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TBX_CORE_SOURCES} ${TBX_CORE_INCLUDES})

target_include_directories(Core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:EnTT,INTERFACE_INCLUDE_DIRECTORIES>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Core
    PUBLIC
        TBX_PLUGIN_ABI_VERSION=${TBX_PLUGIN_ABI_VERSION}
)

target_precompile_headers(Core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/core/pch.h")

target_link_libraries(Core
    PUBLIC
        Tbx::Resources
        EnTT::EnTT
        glm
    PRIVATE
        nlohmann_json
        spdlog::spdlog
)

set(CMAKE_FOLDER "tests")
add_subdirectory(tests)
unset(CMAKE_FOLDER)
