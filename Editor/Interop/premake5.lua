project "Interop"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    warnings "Default"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../" .. OutputDir .. "/bin/%{prj.name}/")
    objdir    ("../../" .. OutputDir .. "/obj/%{prj.name}/")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
        "./**.cs",
    }
    
    -- Setup standard platforms and configs
    IncludeEngine()
    StandardPlatforms()
    StandardConfigs()
    DllConfigs()
    
    -- Editor Supported Platforms
    filter "system:Windows"
        defines
        {
            "TOYBOX_EDITOR"
    }
