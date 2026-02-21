if(NOT DEFINED TBX_COPY_CONFIG)
    message(FATAL_ERROR "TbxCopyAssetsPostBuild: TBX_COPY_CONFIG is required")
endif()
if(NOT DEFINED TBX_COPY_DEST)
    message(FATAL_ERROR "TbxCopyAssetsPostBuild: TBX_COPY_DEST is required")
endif()
if(NOT DEFINED TBX_COPY_SOURCES)
    message(FATAL_ERROR "TbxCopyAssetsPostBuild: TBX_COPY_SOURCES is required")
endif()

if(NOT DEFINED TBX_COPY_CONFIGS OR TBX_COPY_CONFIGS STREQUAL "")
    set(TBX_COPY_CONFIGS Release)
endif()

list(FIND TBX_COPY_CONFIGS "${TBX_COPY_CONFIG}" config_match_index)
if(config_match_index EQUAL -1)
    return()
endif()

file(MAKE_DIRECTORY "${TBX_COPY_DEST}")

foreach(source_dir IN LISTS TBX_COPY_SOURCES)
    if(NOT IS_DIRECTORY "${source_dir}")
        message(WARNING "Toybox: asset source directory not found: ${source_dir}")
        continue()
    endif()

    file(COPY "${source_dir}/" DESTINATION "${TBX_COPY_DEST}")
endforeach()
