add_library(InputExampleRuntime SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../asset_example/assets/*.*")
target_sources(InputExampleRuntime PRIVATE ${SRCS} ${ASSETS})
target_include_directories(InputExampleRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(InputExampleRuntime PRIVATE
    Tbx::DefaultIncludes
    Tbx::Examples::Common
)

tbx_register_plugin(
    TARGET InputExampleRuntime
    CLASS input_example::InputExampleRuntimePlugin
    HEADER "runtime.h"
    NAME "InputExampleRuntime"
    VERSION "1.0.0"
    CATEGORY "gameplay"
    RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/../../asset_example/assets"
)

add_library(Tbx::Examples::InputExampleRuntime ALIAS InputExampleRuntime)
