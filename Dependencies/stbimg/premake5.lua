project "stbimg"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    flags
    {
        "MultiProcessorCompile"
    }

    if OutputIntermediateDir == nil or OutputTargetDir == nil then
        targetdir ("Build/bin/%{prj.name}/")
        objdir    ("Build/obj/%{prj.name}/")

    else
        targetdir ("../../" .. OutputTargetDir .. "")
        objdir    ("../../" .. OutputIntermediateDir .. "")
    end

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

    defines
    {
        "STB_IMAGE_IMPLEMENTATION"
    }

    filter "configurations:Debug"
        runtime "Debug"
        buildoptions { "/MDd" } 
        symbols "On"

    filter "configurations:Optimized"
        runtime "Release"
        buildoptions { "/MDd" } 
        optimize "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"
        buildoptions { "/MD" } 
        symbols "Off"
