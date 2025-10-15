#pragma once

#ifdef TBX_DEBUG
    constexpr bool IsDebugBuild = true;
#else
    constexpr bool IsDebugBuild = false;
#endif

#ifdef TBX_RELEASE
    constexpr bool IsReleaseBuild = true;
#else
    constexpr bool IsReleaseBuild = false;
#endif

#ifdef TBX_PLATFORM_WINDOWS
    constexpr bool IsWindowsBuild = true;
#else
    constexpr bool IsWindowsBuild = false;
#endif

#ifdef TBX_PLATFORM_LINUX
    constexpr bool IsLinuxBuild = true;
#else
    constexpr bool IsLinuxBuild = false;
#endif

#ifdef TBX_PLATFORM_MACOS
    constexpr bool IsMacOSBuild = true;
#else
    constexpr bool IsMacOSBuild = false;
#endif