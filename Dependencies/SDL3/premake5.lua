project "SDL3"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"

    files
    {
        "./**.h",
        "./**.hpp"
    }
    includedirs
    {
        "./include"
    }

    postbuildcommands 
    {
        "{COPYFILE} \"./lib/x64/SDL3.lib\" \"%{OutputTargetDir}\"",
        "{COPYFILE} \"./lib/x64/SDL3.dll\" \"%{OutputTargetDir}\"",
        "{COPYFILE} \"./lib/x64/SDL3.pdb\" \"%{OutputTargetDir}\""
    }