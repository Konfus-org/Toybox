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