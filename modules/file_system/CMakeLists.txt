tbx_add_module(FileSystem)

file(GLOB_RECURSE FILE_SYSTEM_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE FILE_SYSTEM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")

target_sources(FileSystem PRIVATE ${FILE_SYSTEM_SOURCES} ${FILE_SYSTEM_HEADERS})

target_include_directories(FileSystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(FileSystem
    PUBLIC
)

add_library(Tbx::FileSystem ALIAS FileSystem)
