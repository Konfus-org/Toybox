project "GlfwInput"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    warnings "Default"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../" .. OutputDir .. "/bin/Modules/")
    objdir    ("../../" .. OutputDir .. "/obj/Modules/")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
        "./**.cs",
    }

    defines
    {
        "GLFW_INCLUDE_NONE"
    }

    includedirs
    {
        "./",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glad}"
    }

    links
    {
        "glfw",
        "glad",
    }
    
    -- Setup standard platforms and configs
    IncludeEngine()
    StandardPlatforms()
    StandardConfigs()
    
    -- Needed for .DLL
    filter "configurations:Debug"
        buildoptions "/MDd"

    filter "configurations:Optimized"
        buildoptions "/MD"
    
    filter "configurations:Dist"
        buildoptions "/MD"
