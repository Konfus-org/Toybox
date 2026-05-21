option(TBX_BUILD_SHARED "Build Toybox core engine as a shared library" ON)
option(TBX_ENABLE_VERBOSE "Enable verbose Toybox diagnostics" OFF)

set(TBX_PLUGIN_ABI_VERSION 1 CACHE STRING "Plugin ABI version used for plugin compatibility checks" FORCE)

message(STATUS "")
message(STATUS "Configuring Toybox Engine:")

if(CMAKE_CXX_FLAGS MATCHES "-fno-rtti")
    message(FATAL_ERROR "Tbx: RTTI must be enabled for Toybox")
endif()

message(STATUS "Tbx Options:")
message(STATUS "   TBX_BUILD_SHARED: ${TBX_BUILD_SHARED}")
message(STATUS "   TBX_ENABLE_VERBOSE: ${TBX_ENABLE_VERBOSE}")

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

file(GLOB_RECURSE TBX_Engine_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE TBX_Engine_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(Engine PRIVATE ${TBX_Engine_SOURCES} ${TBX_Engine_INCLUDES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${TBX_Engine_SOURCES} ${TBX_Engine_INCLUDES})

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

set(CMAKE_FOLDER "tests")
add_subdirectory(tests)
unset(CMAKE_FOLDER)
