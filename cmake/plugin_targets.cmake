include_guard(GLOBAL)
include(code_gen_utility)

function(tbx_add_plugin)
    set(options)
    set(one_value_args NAME BASE_DIR)
    set(multi_value_args PRIVATE_LINKS PUBLIC_LINKS)
    cmake_parse_arguments(TBX_PLUGIN "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TBX_PLUGIN_NAME)
        message(FATAL_ERROR "tbx_add_plugin: NAME is required")
    endif()
    if(NOT TBX_PLUGIN_BASE_DIR)
        set(TBX_PLUGIN_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    add_library(${TBX_PLUGIN_NAME} SHARED)
    add_library(Tbx::Plugins::${TBX_PLUGIN_NAME} ALIAS ${TBX_PLUGIN_NAME})

    target_compile_features(${TBX_PLUGIN_NAME} PUBLIC cxx_std_23)
    target_include_directories(${TBX_PLUGIN_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${TBX_PLUGIN_BASE_DIR}/include>
    )
    if(TBX_PLUGIN_PUBLIC_LINKS)
        target_link_libraries(${TBX_PLUGIN_NAME} PUBLIC ${TBX_PLUGIN_PUBLIC_LINKS})
    endif()
    target_link_libraries(${TBX_PLUGIN_NAME}
        PRIVATE
            Tbx::Engine
            ${TBX_PLUGIN_PRIVATE_LINKS}
    )
    target_precompile_headers(${TBX_PLUGIN_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/engine/include/tbx/pch.h")

    tbx_codegen_generate_plugin_registration(
        TARGET ${TBX_PLUGIN_NAME}
        BASE_DIR "${TBX_PLUGIN_BASE_DIR}"
    )
endfunction()
