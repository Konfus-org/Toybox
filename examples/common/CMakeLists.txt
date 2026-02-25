set(CMAKE_FOLDER "examples/common")

add_library(ExamplesCommon STATIC)
set_target_properties(ExamplesCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE EXAMPLES_COMMON_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
file(GLOB_RECURSE EXAMPLES_COMMON_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(ExamplesCommon PRIVATE ${EXAMPLES_COMMON_HEADERS} ${EXAMPLES_COMMON_SOURCES})

# Public include path for shared example helpers.
target_include_directories(ExamplesCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ExamplesCommon PUBLIC
    Tbx::DefaultIncludes
)

add_library(Tbx::Examples::Common ALIAS ExamplesCommon)

unset(CMAKE_FOLDER)
