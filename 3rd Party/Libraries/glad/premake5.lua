project "glad"
    kind "StaticLib"
    language "C"

    if OutputIntermediateDir == nil or OutputTargetDir == nil then
        targetdir ("Build/bin/%{prj.name}/")
        objdir    ("Build/obj/%{prj.name}/")

    else
        targetdir ("../../../" .. OutputTargetDir .. "")
        objdir    ("../../../" .. OutputIntermediateDir .. "")
    end

    includedirs
    {
        "include"
    }

    files
    {
        "src/*.c",
        "include/glad/*.h",
        "include/KHR/*.h",
        "./**.md",
        "./**.lua"
    }

    filter "system:windows"
        defines
        {
            "_CRT_SECURE_NO_WARNINGS"
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
