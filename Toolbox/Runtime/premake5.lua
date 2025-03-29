project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxApp}",
        "%{IncludeDir.ModernJSON}"
    }

    links
    {
        "Core",
        "App",
        "ModernJSON"
    }

    ToolboxProjectConfigs()