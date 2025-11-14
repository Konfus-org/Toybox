tbx_add_module(Std)

file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

list(FILTER INCL EXCLUDE REGEX "/file_system/")
list(FILTER SRCS EXCLUDE REGEX "/file_system/")
list(FILTER SRCS EXCLUDE REGEX "/string_path_operations\\.cpp$")

target_sources(Std PRIVATE ${SRCS} ${INCL})

target_include_directories(Std
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(Tbx::Std ALIAS Std)
