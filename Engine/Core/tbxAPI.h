#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #ifdef TOYBOX
        #define TBX_API __declspec(dllexport)
    #else
        #define TBX_API __declspec(dllimport)
    #endif
#else
    #define TBX_API
#endif