if(NOT DEFINED TBX_BUNDLE_CONFIG)
    message(FATAL_ERROR "TbxBundleAssetsPostBuild: TBX_BUNDLE_CONFIG is required")
endif()
if(NOT DEFINED TBX_BUNDLE_DEST)
    message(FATAL_ERROR "TbxBundleAssetsPostBuild: TBX_BUNDLE_DEST is required")
endif()
if(NOT DEFINED TBX_BUNDLE_SOURCES)
    message(FATAL_ERROR "TbxBundleAssetsPostBuild: TBX_BUNDLE_SOURCES is required")
endif()

if(NOT DEFINED TBX_BUNDLE_CONFIGS OR TBX_BUNDLE_CONFIGS STREQUAL "")
    set(TBX_BUNDLE_CONFIGS Release)
endif()

list(FIND TBX_BUNDLE_CONFIGS "${TBX_BUNDLE_CONFIG}" config_match_index)
if(config_match_index EQUAL -1)
    return()
endif()

file(MAKE_DIRECTORY "${TBX_BUNDLE_DEST}")

message(STATUS
    "Toybox asset bundle: config=${TBX_BUNDLE_CONFIG}, dest=${TBX_BUNDLE_DEST}")

foreach(source_dir IN LISTS TBX_BUNDLE_SOURCES)
    if(NOT IS_DIRECTORY "${source_dir}")
        message(WARNING "Toybox: asset bundle source directory not found: ${source_dir}")
        continue()
    endif()

    file(
        GLOB_RECURSE bundled_files
        LIST_DIRECTORIES false
        RELATIVE "${source_dir}"
        "${source_dir}/*"
    )

    set(processed_count 0)
    set(updated_count 0)
    foreach(relative_path IN LISTS bundled_files)
        string(REPLACE "\\" "/" normalized_relative_path "${relative_path}")
        if(normalized_relative_path MATCHES "(^|/)generated(/|$)")
            continue()
        endif()

        set(source_path "${source_dir}/${relative_path}")
        get_filename_component(source_name "${source_path}" NAME)
        string(TOLOWER "${source_name}" lowered_source_name)
        if(lowered_source_name STREQUAL "cmakelists.txt")
            continue()
        endif()

        get_filename_component(source_extension "${source_path}" EXT)
        string(TOLOWER "${source_extension}" lowered_source_extension)
        if(lowered_source_extension STREQUAL ".cmake"
           OR lowered_source_extension STREQUAL ".h"
           OR lowered_source_extension STREQUAL ".hh"
           OR lowered_source_extension STREQUAL ".hpp"
           OR lowered_source_extension STREQUAL ".c"
           OR lowered_source_extension STREQUAL ".cc"
           OR lowered_source_extension STREQUAL ".cpp"
           OR lowered_source_extension STREQUAL ".cxx"
           OR lowered_source_extension STREQUAL ".in")
            continue()
        endif()

        set(destination_path "${TBX_BUNDLE_DEST}/${normalized_relative_path}")
        get_filename_component(destination_directory "${destination_path}" DIRECTORY)
        file(MAKE_DIRECTORY "${destination_directory}")
        math(EXPR processed_count "${processed_count} + 1")
        if(NOT EXISTS "${destination_path}" OR "${source_path}" IS_NEWER_THAN "${destination_path}")
            math(EXPR updated_count "${updated_count} + 1")
        endif()
        file(COPY_FILE "${source_path}" "${destination_path}" ONLY_IF_DIFFERENT)
    endforeach()

    message(STATUS
        "Toybox asset bundle: ${source_dir} -> ${processed_count} files considered, ${updated_count} updated")
endforeach()
