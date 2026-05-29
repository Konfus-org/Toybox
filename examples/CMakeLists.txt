option(TBX_BUILD_EXAMPLES "Build Toybox examples" ON)

function(tbx_add_optional_example example_dir)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${example_dir}/CMakeLists.txt")
        add_subdirectory(${example_dir})
    endif()
endfunction()

if(TBX_BUILD_EXAMPLES)
    set(CMAKE_FOLDER "examples")

    include(code_gen_utility)
    include(default_plugins)
    include(example_targets)
    tbx_define_default_plugins()

    foreach(example_dir IN ITEMS
        2d_example
        input_example
        3d_example
        lighting_example
    )
        tbx_add_optional_example(${example_dir})
    endforeach()

    unset(CMAKE_FOLDER)
endif()
