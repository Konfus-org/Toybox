project "Main"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    externalwarnings "Off"

    targetdir ("../" .. OutputTargetDir .. "")
    objdir    ("../" .. OutputIntermediateDir .. "")

    includedirs
    {
        "%{Using.TbxCore}",
        "%{Using.TbxRuntime}",
        "%{Using.ModernJSON}",
    }

    links
    {
        "Core",
        "Runtime",
        "ModernJSON",
        table.unpack(Using.TbxPluginLinks),
    }

    ToyboxProjectConfigs()