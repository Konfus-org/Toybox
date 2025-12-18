add_library(EcsExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(EcsExampleRuntime PRIVATE ${SRCS})
target_include_directories(EcsExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(EcsExampleRuntime PRIVATE
    Tbx::App
    Tbx::ECS
    Tbx::Plugins::SpdConsoleLoggerPlugin
    Tbx::Plugins::SpdFileLoggerPlugin
    Tbx::Plugins::SdlWindowingPlugin
)

tbx_register_plugin(
    TARGET EcsExampleRuntime
    CLASS tbx::examples::ExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "ExampleRuntimePlugin"
    VERSION "1.0.0"
    TYPE "runtime"
)

add_library(Tbx::Examples::EcsExampleRuntime ALIAS EcsExampleRuntime)
