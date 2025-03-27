project "Simple Test App"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../" .. OutputTargetDir .. "")
    objdir    ("../" .. OutputIntermediateDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    UsingToolboxConfigs()