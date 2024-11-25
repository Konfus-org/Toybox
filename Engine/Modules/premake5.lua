project "Modules"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    warnings "Default"
    
    targetdir ("../../" .. OutputDir .. "/bin/%{prj.name}/")
    objdir    ("../../" .. OutputDir .. "/obj/%{prj.name}/")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
    }

    IncludeEngine()
    StandardPlatforms()
    StandardConfigs()