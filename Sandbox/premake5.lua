project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    entrypoint "mainCRTStartup"
    
    targetdir ("../" .. OutputDir .. "/bin/%{prj.name}/")
    objdir    ("../" .. OutputDir .. "/obj/%{prj.name}/")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }
    
    IncludeEngine()
    StandardPlatforms()
    StandardConfigs()