if(NOT DEFINED DESTINATION OR DESTINATION STREQUAL "")
    message(FATAL_ERROR "copy_existing_runtime_dlls: DESTINATION is required.")
endif()

if(NOT DEFINED SOURCES OR SOURCES STREQUAL "")
    return()
endif()

foreach(runtime_dll IN LISTS SOURCES)
    if(NOT EXISTS "${runtime_dll}")
        continue()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${runtime_dll}" "${DESTINATION}"
        RESULT_VARIABLE copy_result
    )
    if(NOT copy_result EQUAL 0)
        message(FATAL_ERROR "copy_existing_runtime_dlls: Failed copying '${runtime_dll}'.")
    endif()
endforeach()
