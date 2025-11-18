cmake_minimum_required(VERSION 3.26)

message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")

set(CMAKE_FOLDER "Plugins")

add_subdirectory(spd_console_logger)
add_subdirectory(spd_file_logger)
add_subdirectory(sdl_adapter)
add_subdirectory(sdl_windowing)

unset(CMAKE_FOLDER)
message(STATUS "")

