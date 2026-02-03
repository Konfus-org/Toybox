function(tbx_setup_module target_name)
    if(NOT target_name)
        message(FATAL_ERROR "tbx_setup_module: target name is required")
    endif()

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "tbx_setup_module: target '${target_name}' does not exist")
    endif()

    file(GLOB_RECURSE INCL CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.*")
    file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
    target_sources(${target_name} PRIVATE ${SRCS} ${INCL})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SRCS} ${INCL})
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_precompile_headers(${target_name} PRIVATE "${PROJECT_SOURCE_DIR}/modules/common/include/tbx/pch.h")
endfunction()