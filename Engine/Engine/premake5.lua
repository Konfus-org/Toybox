project "Engine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
	warnings "Default"
    
    targetdir ("../../" .. OutputDir .. "/bin/%{prj.name}/")
    objdir    ("../../" .. OutputDir .. "/obj/%{prj.name}/")

    pchheader "tbxpch.h"
    pchsource "tbxpch.cpp"

    defines
    {
        "TOYBOX",
		"GLFW_INCLUDE_NONE"
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    includedirs
    {
        "./",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glad}"
    }

    links
    {
        "spdlog",
        "glfw",
        "glad",
        "opengl32.lib"
    }
	
    -- Setup standard platforms and configs
    StandardPlatforms()
    StandardConfigs()

    -- Expose native platform methods 
	-- which is needed to grab handles of windows for editor
    filter "system:Windows"
        defines
        {
            "GLFW_EXPOSE_NATIVE_WIN32"
        }

    filter "system:Linux"
        defines
        {
            "GLFW_EXPOSE_NATIVE_WAYLAND"
        }

    filter "system:Macosx"
        defines
        {
            "GLFW_EXPOSE_NATIVE_COCOA"
        }
		