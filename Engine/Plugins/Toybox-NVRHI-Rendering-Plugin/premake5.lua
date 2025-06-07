project "NVRHI Renderer"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    local vulkan_sdk = os.getenv("VULKAN_SDK")
    assert(vulkan_sdk, "Vulkan SDK not found! Make sure you have Vulkan installed! After install restart PC to make the SDK env path to be available.")

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

        -- NVRHI includes
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/thirdparty/DirectX-Headers/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/thirdparty/DirectX-Headers/include/directx",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/thirdparty/Vulkan-Headers/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/rtxmu/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/include"
    }
    libdirs
    {
        vulkan_sdk .. "/Lib" 
    }
    links
    {
        "NVRHI",
        "vulkan-1"
    }
    defines
    {
        "NVRHI_WITH_VULKAN",
        "RTXMU_WITH_VULKAN"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "NVRHI_WITH_DX12", "RTXMU_WITH_D3D12", "VK_USE_PLATFORM_WIN32_KHR" }
        links { "d3d12" }

    RegisterDynamicPlugin("NVRHI Renderer")