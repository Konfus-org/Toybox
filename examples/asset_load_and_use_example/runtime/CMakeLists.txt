add_library(AssetLoadAndUseExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(AssetLoadAndUseExampleRuntime PRIVATE ${SRCS})
target_include_directories(AssetLoadAndUseExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(AssetLoadAndUseExampleRuntime PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::ECS
    Tbx::Graphics
    Tbx::Plugins::SpdConsoleLoggerPlugin
    Tbx::Plugins::SpdFileLoggerPlugin
    Tbx::Plugins::SdlWindowingPlugin
    Tbx::Plugins::OpenGlRenderingPlugin
    Tbx::Plugins::StbImageLoaderPlugin
    Tbx::Plugins::AssimpModelLoaderPlugin
)

tbx_register_plugin(
    TARGET AssetLoadAndUseExampleRuntime
    CLASS tbx::examples::AssetLoadAndUseExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "AssetLoadAndUseExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
)

add_library(Tbx::Examples::AssetLoadAndUseExampleRuntime ALIAS AssetLoadAndUseExampleRuntime)
