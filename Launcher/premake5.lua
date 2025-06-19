project "Launcher"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md"
    }
    includedirs
    {
        "./Include",
        "./Source",
        "../Engine/Include",
        table.unpack(Using.TbxStaticPluginDirs)
    }
    links
    {
        "Engine",
        table.unpack(Using.TbxStaticPluginLinks),
        table.unpack(Using.TbxDynamicPluginLinks)
    }
    defines
    {
        "PATH_TO_PLUGINS=\"" .. OutputTargetDir .. "\""
    }