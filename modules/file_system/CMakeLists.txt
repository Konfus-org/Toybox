tbx_add_module(FileSystem)

set(STD_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../std")
set(FILE_SYSTEM_INCLUDE_DIR "${STD_MODULE_DIR}/include/tbx/file_system")
set(FILE_SYSTEM_SOURCE_DIR "${STD_MODULE_DIR}/src/file_system")
set(FILE_SYSTEM_EXTRA_SOURCE "${STD_MODULE_DIR}/src/std/string_path_operations.cpp")

file(GLOB_RECURSE FILE_SYSTEM_HEADERS CONFIGURE_DEPENDS "${FILE_SYSTEM_INCLUDE_DIR}/*.*")
file(GLOB_RECURSE FILE_SYSTEM_SOURCES CONFIGURE_DEPENDS "${FILE_SYSTEM_SOURCE_DIR}/*.*")

list(APPEND FILE_SYSTEM_SOURCES "${FILE_SYSTEM_EXTRA_SOURCE}")
list(REMOVE_DUPLICATES FILE_SYSTEM_SOURCES)

set(PRIVATE_INCLUDE_DIRS
    "${STD_MODULE_DIR}/src/file_system"
    "${STD_MODULE_DIR}/src/std"
)

set(PUBLIC_INCLUDE_DIRS
    "$<BUILD_INTERFACE:${STD_MODULE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)

if(FILE_SYSTEM_HEADERS)
    target_sources(FileSystem PRIVATE ${FILE_SYSTEM_HEADERS})
endif()

target_sources(FileSystem PRIVATE ${FILE_SYSTEM_SOURCES})

foreach(dir IN LISTS PRIVATE_INCLUDE_DIRS)
    if(EXISTS "${dir}")
        list(APPEND VALID_PRIVATE_DIRS "${dir}")
    endif()
endforeach()

if(VALID_PRIVATE_DIRS)
    target_include_directories(FileSystem PRIVATE ${VALID_PRIVATE_DIRS})
endif()

target_include_directories(FileSystem PUBLIC ${PUBLIC_INCLUDE_DIRS})

add_library(Tbx::FileSystem ALIAS FileSystem)
