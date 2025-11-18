tbx_add_module(FileSystem)

set(FILE_SYSTEM_EXTRA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../std/src/std/string_path_operations.cpp")

file(GLOB_RECURSE FILE_SYSTEM_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE FILE_SYSTEM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

list(APPEND FILE_SYSTEM_SOURCES "${FILE_SYSTEM_EXTRA_SOURCE}")
list(REMOVE_DUPLICATES FILE_SYSTEM_SOURCES)

target_sources(FileSystem PRIVATE ${FILE_SYSTEM_SOURCES} ${FILE_SYSTEM_HEADERS})

target_include_directories(FileSystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../std/src/std
)

target_link_libraries(FileSystem
    PUBLIC
        Tbx::Std
)

add_library(Tbx::FileSystem ALIAS FileSystem)
