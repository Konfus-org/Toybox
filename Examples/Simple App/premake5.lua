project "Simple App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }
    externalwarnings "Off"

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md",
        "./**.lua",
        "./**.txt",
        "./**.plugin"
    }
    includedirs
    {
        "%{Using.TbxCore}",
        "%{Using.TbxRuntime}"
    }

    links
    {
        "Core",
        "Runtime"
    }

    RegisterDynamicPlugin("Simple App")