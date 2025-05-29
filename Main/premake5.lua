project "Main"
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
        "./**.md",
        "./**.lua",
        "./**.txt"
    }
    includedirs
    {
        "./Include",
        "./Source",
        "../Core/Include",
        "../Runtime/Include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "Core",
        "Runtime",
        table.unpack(Using.TbxPluginLinks),
        table.unpack(Using.TbxCorePluginLinks)
    }