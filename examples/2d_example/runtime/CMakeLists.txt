add_library(TwoDExampleRuntime SHARED)

target_link_libraries(TwoDExampleRuntime PRIVATE
    Tbx::DefaultPlugins
)
target_precompile_headers(TwoDExampleRuntime PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

tbx_codegen_generate_plugin_registration(
    TARGET TwoDExampleRuntime
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_library(Tbx::Examples::TwoDExampleRuntime ALIAS TwoDExampleRuntime)
