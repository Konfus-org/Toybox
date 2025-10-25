cmake_minimum_required(VERSION 3.26)

set(CMAKE_FOLDER "Examples")

# Executable: SpdLoggerExample
add_executable(SpdLoggerExample
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(SpdLoggerExample
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(SpdLoggerExample PRIVATE
    Engine
    spdlog::spdlog
)

target_compile_definitions(SpdLoggerExample PRIVATE SPDLOGGER_PLUGIN_DIR="$<TARGET_FILE_DIR:SpdLoggerPlugin>")

unset(CMAKE_FOLDER)
