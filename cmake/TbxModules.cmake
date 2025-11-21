include_guard(GLOBAL)

function(tbx_add_module target_name)
    if(NOT target_name)
        message(FATAL_ERROR "tbx_add_module: target name is required")
    endif()

    if(TARGET ${target_name})
        message(FATAL_ERROR "tbx_add_module: target '${target_name}' already exists")
    endif()

    if(TBX_BUILD_SHARED)
        add_library(${target_name} SHARED)
        target_compile_definitions(${target_name}
            PUBLIC
                TBX_SHARED_LIB
            PRIVATE
                TBX_EXPORTING_SYMBOLS
        )
    else()
        add_library(${target_name} STATIC)
    endif()

    set(runtime_output_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(runtime_output_directory AND NOT runtime_output_directory MATCHES "\$<CONFIG>")
        set(runtime_output_directory "${runtime_output_directory}/$<CONFIG>")
    endif()

    set(library_output_directory "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    if(library_output_directory AND NOT library_output_directory MATCHES "\$<CONFIG>")
        set(library_output_directory "${library_output_directory}/$<CONFIG>")
    endif()

    set(archive_output_directory "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    if(archive_output_directory AND NOT archive_output_directory MATCHES "\$<CONFIG>")
        set(archive_output_directory "${archive_output_directory}/$<CONFIG>")
    endif()

    set(pdb_output_directory "${CMAKE_PDB_OUTPUT_DIRECTORY}")
    if(pdb_output_directory AND NOT pdb_output_directory MATCHES "\$<CONFIG>")
        set(pdb_output_directory "${pdb_output_directory}/$<CONFIG>")
    endif()

    set_target_properties(${target_name}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${runtime_output_directory}"
            LIBRARY_OUTPUT_DIRECTORY "${library_output_directory}"
            ARCHIVE_OUTPUT_DIRECTORY "${archive_output_directory}"
            PDB_OUTPUT_DIRECTORY "${pdb_output_directory}"
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
            POSITION_INDEPENDENT_CODE ON
    )
endfunction()
