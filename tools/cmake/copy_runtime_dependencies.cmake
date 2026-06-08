if(NOT DEFINED DESTINATION OR DESTINATION STREQUAL "")
    message(FATAL_ERROR "copy_runtime_dependencies: DESTINATION is required")
endif()

if(NOT DEFINED SOURCES OR SOURCES STREQUAL "")
    return()
endif()

file(MAKE_DIRECTORY "${DESTINATION}")

foreach(runtime_dependency IN LISTS SOURCES)
    if(runtime_dependency STREQUAL "" OR NOT EXISTS "${runtime_dependency}")
        continue()
    endif()

    get_filename_component(runtime_dependency_name "${runtime_dependency}" NAME)
    file(COPY_FILE
        "${runtime_dependency}"
        "${DESTINATION}/${runtime_dependency_name}"
        ONLY_IF_DIFFERENT
    )
endforeach()
