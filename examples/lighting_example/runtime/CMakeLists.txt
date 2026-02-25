add_library(LightingExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(LightingExampleRuntime PRIVATE ${SRCS})
target_include_directories(LightingExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(LightingExampleRuntime PRIVATE
    Tbx::DefaultIncludes
    Tbx::Examples::Common
)

tbx_register_plugin(
    TARGET LightingExampleRuntime
    CLASS tbx::examples::LightingExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "LightingExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
    RESOURCES
        "${PROJECT_SOURCE_DIR}/resources"
)

add_library(Tbx::Examples::LightingExampleRuntime ALIAS LightingExampleRuntime)
