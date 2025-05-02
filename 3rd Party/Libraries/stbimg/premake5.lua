project "stbimg"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    if OutputIntermediateDir == nil or OutputTargetDir == nil then
        targetdir ("Build/bin/%{prj.name}/")
        objdir    ("Build/obj/%{prj.name}/")

    else
        targetdir ("../../../" .. OutputTargetDir .. "")
        objdir    ("../../../" .. OutputIntermediateDir .. "")
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
        linkoptions { "/MDd" }  -- Enable Link Time Code Generation and Incremental linking
        symbols "on"

    filter "configurations:Optimized"
        runtime "Release"
        linkoptions { "/MDd" }  -- Enable Link Time Code Generation and Incremental linking
        optimize "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        linkoptions { "/MD" }  -- Enable Link Time Code Generation and Incremental linking
        symbols "off"
