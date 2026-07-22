# tbx_backend(<SUBSYSTEM> <default>)
#
# The generic backend-swap mechanism. Each subsystem that wraps a third-party
# library declares one concrete boundary header (e.g. gpu/gpu.h) and one folder
# per backend implementing it (e.g. gpu/gl/, gpu/vulkan/). This helper:
#
#   * declares the cache variable TBX_<SUBSYSTEM>_BACKEND (default <default>)
#   * validates that engine/src/<subsystem>/<choice>/ exists
#   * collects that folder's sources into TBX_<SUBSYSTEM>_BACKEND_SOURCES
#
# Adding a new backend = dropping a folder that implements the boundary header
# (plus its FetchContent block in the root CMakeLists). It automatically becomes
# a valid -DTBX_<SUBSYSTEM>_BACKEND=<name> option. Backend library types must
# never escape their folder; the boundary header is the wall.
# An optional third argument names the parent folder when it differs from the subsystem
# (e.g. tbx_backend(MATH glm core) selects engine/src/core/<backend>).
function(tbx_backend subsystem default)
    string(TOUPPER "${subsystem}" upper)
    string(TOLOWER "${subsystem}" lower)
    set(var "TBX_${upper}_BACKEND")
    set(parent "${lower}")
    if(ARGC GREATER 2)
        set(parent "${ARGV2}")
    endif()

    set(${var} "${default}" CACHE STRING "Backend for the ${lower} subsystem")

    set(backend_dir "${CMAKE_SOURCE_DIR}/engine/src/${parent}/${${var}}")
    if(NOT IS_DIRECTORY "${backend_dir}")
        file(GLOB available RELATIVE "${CMAKE_SOURCE_DIR}/engine/src/${parent}"
            "${CMAKE_SOURCE_DIR}/engine/src/${parent}/*")
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

# tbx_backend_list(<SUBSYSTEM> <default...>)
#
# The additive variant of tbx_backend() for subsystems where several backends coexist
# (scripting: C++, Lua, and C# at the same time). Declares the cache LIST variable
# TBX_<SUBSYSTEM>_BACKENDS, validates each entry's folder, collects every folder's sources
# into TBX_<SUBSYSTEM>_BACKEND_SOURCES, and defines TBX_<SUBSYSTEM>_HAS_<ENTRY> per entry so
# the coordinator can construct the compiled-in backends.
function(tbx_backend_list subsystem)
    string(TOUPPER "${subsystem}" upper)
    string(TOLOWER "${subsystem}" lower)
    set(var "TBX_${upper}_BACKENDS")

    set(${var} "${ARGN}" CACHE STRING "Backends for the ${lower} subsystem (list)")

    set(sources "")
    foreach(backend IN LISTS ${var})
        set(backend_dir "${CMAKE_SOURCE_DIR}/engine/src/${lower}/${backend}")
        if(NOT IS_DIRECTORY "${backend_dir}")
            message(FATAL_ERROR "${var} entry '${backend}': '${backend_dir}' does not exist")
        endif()
        file(GLOB_RECURSE backend_sources CONFIGURE_DEPENDS
            "${backend_dir}/*.cpp" "${backend_dir}/*.c" "${backend_dir}/*.h")
        list(APPEND sources ${backend_sources})
        string(TOUPPER "${backend}" backend_upper)
        add_compile_definitions("TBX_${upper}_HAS_${backend_upper}")
    endforeach()
    set(TBX_${upper}_BACKEND_SOURCES "${sources}" PARENT_SCOPE)
    message(STATUS "Toybox ${lower} backends: ${${var}}")
endfunction()
