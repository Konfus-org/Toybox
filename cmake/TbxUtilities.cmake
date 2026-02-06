# Add utilities
set(CMAKE_FOLDER "utility")
set(TBX_COMPILE_COMMANDS_SOURCE "${CMAKE_BINARY_DIR}/compile_commands.json")
set(TBX_COMPILE_COMMANDS_DESTINATION "${CMAKE_SOURCE_DIR}/compile_commands.json")
add_custom_target(CompileCommandsLink ALL
    COMMAND ${CMAKE_COMMAND}
        -D "TBX_COMPILE_COMMANDS_SOURCE=${TBX_COMPILE_COMMANDS_SOURCE}"
        -D "TBX_COMPILE_COMMANDS_DESTINATION=${TBX_COMPILE_COMMANDS_DESTINATION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyCompileCommands.cmake"
    BYPRODUCTS "${TBX_COMPILE_COMMANDS_DESTINATION}"
    COMMENT "Sync compile_commands.json into the repo root for clangd."
)
add_custom_target(Tests
    DEPENDS
        TbxCommonTests
        TbxFileSystemTests
        TbxMathTests
        TbxAsyncTests
        TbxTimeTests
        TbxMessagingTests
        TbxPluginApiTests
        TbxECSTests
        TbxGraphicsTests
        TbxAppTests
)
unset(CMAKE_FOLDER)
