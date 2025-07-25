project "SDL Input"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    RegisterDynamicPlugin("SDL Input")

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
        _MAIN_SCRIPT_DIR .. "/Dependencies/SDL3/include"
    }
    links
    {
        "SDL3"
    }
