project "Simple App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    externalwarnings "Off"

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.png",
        "./**.jpg",
        "./**.frag",
        "./**.vert",
        "./**.md"
    }
    includedirs
    {
        "./"
    }

    RegisterDynamicPlugin("Simple App")