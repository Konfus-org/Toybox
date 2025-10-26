add_library(SpdLoggerPlugin SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/spdlogger_plugin.cpp
)

target_include_directories(SpdLoggerPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SpdLoggerPlugin PRIVATE
    Engine
    spdlog::spdlog
)

# Copy manifest next to the built library
add_custom_command(TARGET SpdLoggerPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/plugin.meta
            $<TARGET_FILE_DIR:SpdLoggerPlugin>/SpdLoggerPlugin.meta)

add_library(Tbx::Plugins::SpdLoggerPlugin ALIAS SpdLoggerPlugin)

