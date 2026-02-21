add_library(AssetExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/*.*")
target_sources(AssetExampleRuntime PRIVATE ${SRCS} ${ASSETS})
target_include_directories(AssetExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(AssetExampleRuntime PRIVATE
    Tbx::DefaultIncludes
)

tbx_register_plugin(
    TARGET AssetExampleRuntime
    CLASS tbx::examples::AssetExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "AssetExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
    RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../assets"
)

add_library(Tbx::Examples::AssetExampleRuntime ALIAS AssetExampleRuntime)

