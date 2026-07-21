# Engine code compiles with warnings-as-errors (CMAKE_COMPILE_WARNING_AS_ERROR in the presets).
# Vendored third-party code is not ours to fix, so after the whole tree is configured every
# target created under a third-party root (the in-tree thirdparty/ libs and the FetchContent
# cache) gets warnings-as-errors turned off and its diagnostics fully silenced.

function(tbx_silence_third_party_warnings directory)
    set(third_party_roots "${CMAKE_SOURCE_DIR}/thirdparty" "${FETCHCONTENT_BASE_DIR}")
    set(directory_is_third_party FALSE)
    foreach(third_party_root IN LISTS third_party_roots)
        cmake_path(IS_PREFIX third_party_root "${directory}" NORMALIZE root_is_prefix)
        if(root_is_prefix)
            set(directory_is_third_party TRUE)
        endif()
    endforeach()

    if(directory_is_third_party)
        if(MSVC)
            set(silence_flag /w)
        else()
            set(silence_flag -w)
        endif()

        # A warning-level flag followed by the silence flag makes cl emit D9025 ("overriding
        # /W4 with /w"), so warning-level flags are dropped rather than merely overridden —
        # both plain ("/W3") and generator-expression-wrapped ("$<...:/W3>", SDL's style).
        set(warning_level_regex "^[/-]W(all|[0-4])$|:[/-]W(all|[0-4])>$")

        get_property(directory_options DIRECTORY "${directory}" PROPERTY COMPILE_OPTIONS)
        if(directory_options)
            list(FILTER directory_options EXCLUDE REGEX "${warning_level_regex}")
            set_property(DIRECTORY "${directory}" PROPERTY COMPILE_OPTIONS "${directory_options}")
        endif()

        get_property(directory_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(directory_target IN LISTS directory_targets)
            get_target_property(target_type ${directory_target} TYPE)
            if(target_type STREQUAL "INTERFACE_LIBRARY" OR target_type STREQUAL "UTILITY")
                continue()
            endif()
            set_target_properties(${directory_target} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
            get_target_property(target_options ${directory_target} COMPILE_OPTIONS)
            if(NOT target_options)
                set(target_options "")
            endif()
            list(FILTER target_options EXCLUDE REGEX "${warning_level_regex}")
            list(APPEND target_options "${silence_flag}")
            set_target_properties(${directory_target} PROPERTIES COMPILE_OPTIONS "${target_options}")
        endforeach()
    endif()

    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(subdirectory IN LISTS subdirectories)
        tbx_silence_third_party_warnings("${subdirectory}")
    endforeach()
endfunction()

cmake_language(DEFER CALL tbx_silence_third_party_warnings "${CMAKE_SOURCE_DIR}")
