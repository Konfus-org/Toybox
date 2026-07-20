# tbx_backend(<SUBSYSTEM> <default>)
#
# The generic backend-swap mechanism. Each subsystem that wraps a third-party
# library declares one concrete boundary header (e.g. gfx/gpu.h) and one folder
# per backend implementing it (e.g. gfx/gl/, gfx/vulkan/). This helper:
#
#   * declares the cache variable TBX_<SUBSYSTEM>_BACKEND (default <default>)
#   * validates that engine/src/<subsystem>/<choice>/ exists
#   * collects that folder's sources into TBX_<SUBSYSTEM>_BACKEND_SOURCES
#
# Adding a new backend = dropping a folder that implements the boundary header
# (plus its FetchContent block in the root CMakeLists). It automatically becomes
# a valid -DTBX_<SUBSYSTEM>_BACKEND=<name> option. Backend library types must
# never escape their folder; the boundary header is the wall.
function(tbx_backend subsystem default)
    string(TOUPPER "${subsystem}" upper)
    string(TOLOWER "${subsystem}" lower)
    set(var "TBX_${upper}_BACKEND")

    set(${var} "${default}" CACHE STRING "Backend for the ${lower} subsystem")

    set(backend_dir "${CMAKE_SOURCE_DIR}/engine/src/${lower}/${${var}}")
    if(NOT IS_DIRECTORY "${backend_dir}")
        file(GLOB available RELATIVE "${CMAKE_SOURCE_DIR}/engine/src/${lower}"
            "${CMAKE_SOURCE_DIR}/engine/src/${lower}/*")
        set(options "")
        foreach(entry IN LISTS available)
            if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/engine/src/${lower}/${entry}")
                list(APPEND options "${entry}")
            endif()
        endforeach()
        message(FATAL_ERROR
            "${var}=${${var}} but '${backend_dir}' does not exist. "
            "Available backends: ${options}")
    endif()

    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${backend_dir}/*.cpp" "${backend_dir}/*.c" "${backend_dir}/*.h")
    set(TBX_${upper}_BACKEND_SOURCES "${sources}" PARENT_SCOPE)
    message(STATUS "Toybox ${lower} backend: ${${var}}")
endfunction()
