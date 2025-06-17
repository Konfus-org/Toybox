project "Simple App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    externalwarnings "Off"

    RegisterDynamicPlugin("Simple App")

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md"
    }