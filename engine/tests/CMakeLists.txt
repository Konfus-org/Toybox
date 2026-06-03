set(CMAKE_FOLDER "tests")

foreach(test_dir IN ITEMS
    app
    assets
    async
    files
    math
    input
    messaging
    plugin_api
    ecs
    graphics
    attributes
    scripting
    time
    utils
)
    add_subdirectory(${test_dir})
endforeach()

unset(CMAKE_FOLDER)
