add_library(AssetLoadAndUseExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(AssetLoadAndUseExampleRuntime PRIVATE ${SRCS})
target_include_directories(AssetLoadAndUseExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(AssetLoadAndUseExampleRuntime PRIVATE
    Tbx::DefaultIncludes
    Tbx::Plugins::SpdLoggerPlugin
    Tbx::Plugins::SdlWindowingPlugin
    Tbx::Plugins::OpenGlRenderingPlugin
    Tbx::Plugins::StbImageLoaderPlugin
    Tbx::Plugins::AssimpModelLoaderPlugin
    Tbx::Plugins::SdrShaderLoaderPlugin
    Tbx::Plugins::MatMaterialLoaderPlugin
)

tbx_register_plugin(
    TARGET AssetLoadAndUseExampleRuntime
    CLASS tbx::examples::AssetLoadAndUseExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "AssetLoadAndUseExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
    RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../assets"
)

add_library(Tbx::Examples::AssetLoadAndUseExampleRuntime ALIAS AssetLoadAndUseExampleRuntime)
