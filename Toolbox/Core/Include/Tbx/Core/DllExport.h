#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #ifdef TOOLBOX
        #define EXPORT __declspec(dllexport)
    #else
        #define EXPORT __declspec(dllimport)
    #endif
#else
    #define EXPORT
#endif