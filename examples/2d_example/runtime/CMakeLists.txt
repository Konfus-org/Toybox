add_library(TwoDExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(TwoDExampleRuntime PRIVATE ${SRCS})
target_include_directories(TwoDExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(TwoDExampleRuntime PRIVATE
    Tbx::DefaultPlugins
)

tbx_register_plugin(
    TARGET TwoDExampleRuntime
    CLASS two_d_example::TwoDExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "TwoDExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
)

add_library(Tbx::Examples::TwoDExampleRuntime ALIAS TwoDExampleRuntime)
