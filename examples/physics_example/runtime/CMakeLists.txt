add_library(PhysicsExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../asset_example/assets/*.*")
target_sources(PhysicsExampleRuntime PRIVATE ${SRCS} ${ASSETS})
target_include_directories(PhysicsExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(PhysicsExampleRuntime PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::ECS
    Tbx::Graphics
    Tbx::Physics
    Tbx::Plugins::JoltPhysicsPlugin
    Tbx::Plugins::SdlWindowingPlugin
    Tbx::Plugins::SdlInputPlugin
    Tbx::Plugins::OpenGlRenderingPlugin
    Tbx::Plugins::SdlOpenGlAdapterPlugin
    Tbx::Plugins::StbImageLoaderPlugin
    Tbx::Plugins::AssimpModelLoaderPlugin
    Tbx::Plugins::GlslShaderLoaderPlugin
    Tbx::Plugins::MatMaterialLoaderPlugin
)

tbx_register_plugin(
    TARGET PhysicsExampleRuntime
    CLASS tbx::examples::PhysicsExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "PhysicsExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
    RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../../asset_example/assets"
)

add_library(Tbx::Examples::PhysicsExampleRuntime ALIAS PhysicsExampleRuntime)
