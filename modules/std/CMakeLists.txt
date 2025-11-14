add_library(Std STATIC)

set(STD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/filesystem_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/string_path_operations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uuid.cpp
)

set(STD_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/tbx_api.h
)

file(GLOB STD_PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/std/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/tbx/std/*.inl"
)

list(APPEND STD_HEADERS ${STD_PUBLIC_HEADERS})

target_sources(Std PRIVATE ${STD_SOURCES} ${STD_HEADERS})

target_include_directories(Std
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Std PRIVATE TBX_EXPORTING_SYMBOLS=1)

tbx_apply_common_definitions(Std)

add_library(Tbx::Std ALIAS Std)
