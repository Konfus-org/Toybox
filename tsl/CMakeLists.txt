add_library(TbxStdWrappers STATIC)

target_sources(TbxStdWrappers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tbx_std_wrappers_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tsl/string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tsl/uuid.cpp
)

target_include_directories(TbxStdWrappers
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/core/include
)

if(TBX_BUILD_SHARED)
    target_compile_definitions(TbxStdWrappers PRIVATE TBX_SHARED_LIB=1)
endif()

target_compile_definitions(TbxStdWrappers PRIVATE TBX_EXPORTING_SYMBOLS=1)

tbx_register_plugin(
    TARGET TbxStdWrappers
    CLASS tbx::TbxStdWrappersPlugin
    HEADER "src/tbx_std_wrappers_plugin.h"
    NAME "TbxStdWrappers"
    VERSION "1.0.0"
    TYPE "system"
    STATIC
)

add_library(Tbx::Plugins::TbxStdWrappers ALIAS TbxStdWrappers)

target_link_libraries(Core PUBLIC TbxStdWrappers)
