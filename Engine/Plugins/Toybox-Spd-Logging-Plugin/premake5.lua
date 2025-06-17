project "Spd Logging"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md",
        "./**.plugin"
    }
    includedirs
    {
        "./Source",
        _MAIN_SCRIPT_DIR .. "/Dependencies/spdlog/include"
    }
    links
    {
        "spdlog"
    }

    RegisterDynamicPlugin("Spd Logging")
