project "stbimg"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    files
    {
        "src/*.cpp",
        "include/*.h",
        "./**.md",
        "./**.lua",
    }
    includedirs
    {
        "include"
    }
