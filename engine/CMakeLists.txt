option(TBX_BUILD_SHARED "Build Toybox core engine as a shared library" ON)
option(TBX_ENABLE_VERBOSE "Enable verbose Toybox diagnostics" OFF)

set(TBX_PLUGIN_ABI_VERSION 1 CACHE STRING "Plugin ABI version used for plugin compatibility checks" FORCE)

message(STATUS "")
message(STATUS "Configuring Toybox Engine:")

# The engine core owns its third-party dependencies. They are fetched here so the include
# directories referenced below (glm/EnTT) and the link list at the bottom resolve.
include(FetchContent)

# EnTT (header-only ECS) — used publicly by the engine and by several plugins.
if(NOT TARGET EnTT::EnTT)
    set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.14.0
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(entt)
endif()

# glm (header-only math) — used publicly by the engine and by the physics plugin.
if(NOT TARGET glm)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    # glm 1.0.1 declares a cmake_minimum_required old enough that CMake prints deprecation
    # warnings; raising the policy floor suppresses them without touching glm.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(glm)
endif()

# nlohmann_json (header-only) — engine serialization.
if(NOT TARGET nlohmann_json)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# spdlog (compiled) — engine logging. v1.15.3 bundles an fmt new enough to pass its own
# compile-time format-string checks under recent clang.
if(NOT TARGET spdlog::spdlog)
    set(SPDLOG_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
        GIT_SHALLOW TRUE
        SYSTEM)
    FetchContent_MakeAvailable(spdlog)
endif()

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

include(codegen)

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
    OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated/include"
    INCLUDE_SCOPE PUBLIC
)

tbx_codegen_generate_attribute_headers(
    TARGET Engine
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src"
    OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated/src"
    INCLUDE_SCOPE PRIVATE
)

target_compile_definitions(Engine
    PUBLIC
        TBX_PLUGIN_ABI_VERSION=${TBX_PLUGIN_ABI_VERSION}
    PRIVATE
        # spdlog v1.15.3 bundles fmt v11.2.0, whose FMT_USE_CONSTEVAL=1 path rejects spdlog's own
        # FMT_STRING construction under recent clang (spdlog/logger.h:334). Disable the consteval
        # format-string check (runtime checks remain). Scoped PRIVATE: logging.cpp is the only TU that
        # includes fmt/spdlog, so no other target needs it.
        FMT_USE_CONSTEVAL=0
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
