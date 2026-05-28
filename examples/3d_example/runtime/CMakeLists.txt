add_library(ThreeDExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/*.*")
target_sources(ThreeDExampleRuntime PRIVATE ${SRCS} ${ASSETS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SRCS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../assets" PREFIX "assets" FILES ${ASSETS})
target_include_directories(ThreeDExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ThreeDExampleRuntime PRIVATE
    Tbx::DefaultPlugins
)

tbx_register_plugin(
    TARGET ThreeDExampleRuntime
    CLASS three_d_example::ThreeDExampleRuntimePlugin
    HEADER "threed_example.h"
    NAME "ThreeDExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
)

add_library(Tbx::Examples::ThreeDExampleRuntime ALIAS ThreeDExampleRuntime)
