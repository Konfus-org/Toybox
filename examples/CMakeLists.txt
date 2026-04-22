option(TBX_BUILD_EXAMPLES "Build Toybox examples" ON)

if (TBX_BUILD_EXAMPLES)
    set(CMAKE_FOLDER "examples")

    include(default_plugins)
    tbx_define_default_plugins()

    add_subdirectory(2d_example)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/input_example/CMakeLists.txt")
        add_subdirectory(input_example)
    endif()
    add_subdirectory(3d_example)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lighting_example/CMakeLists.txt")
        add_subdirectory(lighting_example)
    endif()

    unset(CMAKE_FOLDER)
endif()
