#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT
#endif