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
        "include/KHR/*.h"
    }

    filter "system:windows"
        defines
        {
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "configurations:Debug"
        runtime "Debug"
        linkoptions { "/INCREMENTAL", "/LTCG:incremental" }  -- Enable Link Time Code Generation and Incremental linking
        symbols "on"

    filter "configurations:Optimized"
        runtime "Release"
        linkoptions { "/INCREMENTAL", "/LTCG:incremental" }  -- Enable Link Time Code Generation and Incremental linking
        optimize "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
        linkoptions { "/INCREMENTAL", "/LTCG:incremental" }  -- Enable Link Time Code Generation and Incremental linking
        symbols "off"
