add_library(ThreeDExampleRuntime SHARED)

file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/*.*")
target_sources(ThreeDExampleRuntime PRIVATE ${ASSETS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../assets" PREFIX "assets" FILES ${ASSETS})

target_link_libraries(ThreeDExampleRuntime PRIVATE
    Tbx::DefaultPlugins
)
target_precompile_headers(ThreeDExampleRuntime PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

tbx_register_asset_bundle_path(
    TARGET ThreeDExampleRuntime
    PATH "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
)
tbx_enable_release_asset_bundling(
    TARGET ThreeDExampleRuntime
    CONFIGS Debug Release
)

tbx_codegen_generate_plugin_registration(
    TARGET ThreeDExampleRuntime
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_library(Tbx::Examples::ThreeDExampleRuntime ALIAS ThreeDExampleRuntime)
