project "glad"
    kind "StaticLib"
    language "C"
    staticruntime "Off"
    
    targetdir ("" .. ThirdPartyOutputDir .. "/bin/%{prj.name}")
    objdir    ("" .. ThirdPartyOutputDir .. "/obj/%{prj.name}")

    files
    {
        "include/glad/glad.h",
        "include/KHR/khrplatform.h",
        "src/glad.c"
    }

    includedirs
    {
        "include"
    }
    
    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"