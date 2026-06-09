include_guard(GLOBAL)

function(tbx_log_cache_options)
    get_cmake_property(cache_variables CACHE_VARIABLES)
    list(FILTER cache_variables INCLUDE REGEX "^TBX_")
    list(SORT cache_variables)

    message(STATUS "Tbx: cache options:")
    foreach(cache_variable IN LISTS cache_variables)
        get_property(cache_value CACHE "${cache_variable}" PROPERTY VALUE)
        message(STATUS "  ${cache_variable}=${cache_value}")
    endforeach()
endfunction()

tbx_log_cache_options()
