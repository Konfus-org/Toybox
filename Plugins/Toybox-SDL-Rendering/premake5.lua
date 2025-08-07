project "SDL3 Rendering"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    -- Check if vulkan is installed, we need it for shadercross...
    local vulkanPath = os.getenv("VULKAN_SDK")
    if not vulkanPath then
        error("❌ Vulkan SDK not found! Please install it before proceeding.")
    end

    RegisterDynamicPlugin("SDL3 Rendering")

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md",
        "./**.plugin"
    }
    includedirs
    {
        "./Source",
        _MAIN_SCRIPT_DIR .. "/Dependencies/SDL/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/SDL_Shadercross/include",
        vulkanPath
    }
    links
    {
        "SDL3",
        "SDL3_shadercross",
        "dxcompiler"
    }
