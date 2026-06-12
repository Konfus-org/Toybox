message(STATUS "")
message(STATUS "Configuring Toybox Apps:")

set(CMAKE_FOLDER "apps")

add_subdirectory(toybox_app)

unset(CMAKE_FOLDER)
