add_library(EcsExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(EcsExampleRuntime PRIVATE ${SRCS})
target_include_directories(EcsExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(EcsExampleRuntime PRIVATE
    Tbx::DefaultIncludes
    Tbx::Examples::Common
)

tbx_register_plugin(
    TARGET EcsExampleRuntime
    CLASS ecs_example::ExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "EcsExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
    RESOURCES
        "${PROJECT_SOURCE_DIR}/resources"
)

add_library(Tbx::Examples::EcsExampleRuntime ALIAS EcsExampleRuntime)
