option(TBX_BUILD_SHARED "Build Toybox core engine as a shared library" ON)
option(TBX_ENABLE_VERBOSE "Enable verbose Toybox diagnostics" OFF)

set(TBX_PLUGIN_ABI_VERSION 1 CACHE STRING "Plugin ABI version used for plugin compatibility checks" FORCE)

message(STATUS "")
message(STATUS "Configuring Toybox Engine:")

if(CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "Tbx: RTTI must be enabled for Toybox")
endif()

get_cmake_property(tbx_cache_variables CACHE_VARIABLES)
list(FILTER tbx_cache_variables INCLUDE REGEX "^TBX_")
list(SORT tbx_cache_variables)

message(STATUS "Tbx Options:")
foreach(tbx_cache_variable IN LISTS tbx_cache_variables)
    get_property(tbx_cache_value CACHE "${tbx_cache_variable}" PROPERTY VALUE)
    message(STATUS "   ${tbx_cache_variable}: ${tbx_cache_value}")
endforeach()

if(TBX_BUILD_SHARED)
    add_library(Engine SHARED)
    target_compile_definitions(Engine
        PUBLIC
            TBX_SHARED_LIB
        PRIVATE
            TBX_EXPORTING_SYMBOLS
    )
else()
    add_library(Engine STATIC)
endif()

add_library(Tbx::Engine ALIAS Engine)
set_target_properties(Engine PROPERTIES FOLDER "engine")

include(code_gen_utility)

file(GLOB_RECURSE TBX_Engine_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE TBX_Engine_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
list(FILTER TBX_Engine_INCLUDES EXCLUDE REGEX "\\.tmp[0-9A-Za-z]*$")
list(FILTER TBX_Engine_SOURCES EXCLUDE REGEX "\\.tmp[0-9A-Za-z]*$")

target_sources(Engine PRIVATE ${TBX_Engine_SOURCES} ${TBX_Engine_INCLUDES})
target_sources(Engine
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/systems/plugin_api/plugin_loader_discovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/systems/plugin_api/plugin_loader_discovery.h"
)

target_compile_features(Engine PUBLIC cxx_std_23)
target_include_directories(Engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:EnTT,INTERFACE_INCLUDE_DIRECTORIES>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

tbx_codegen_generate_attribute_headers(
    TARGET Engine
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include"
    OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated/include"
    INCLUDE_SCOPE PUBLIC
)

tbx_codegen_generate_attribute_headers(
    TARGET Engine
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src"
    OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated/src"
    INCLUDE_SCOPE PRIVATE
)

target_compile_definitions(Engine
    PUBLIC
        TBX_PLUGIN_ABI_VERSION=${TBX_PLUGIN_ABI_VERSION}
)

if(TBX_ENABLE_VERBOSE)
    target_compile_definitions(Engine
        PUBLIC
            TBX_ENABLE_VERBOSE
    )
endif()

target_precompile_headers(Engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/pch.h")

target_link_libraries(Engine
    PUBLIC
        Tbx::Resources
        EnTT::EnTT
        glm
        nlohmann_json
    PRIVATE
        spdlog::spdlog
)
