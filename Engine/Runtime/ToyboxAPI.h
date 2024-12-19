#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #ifdef TOYBOX
        #define TOYBOX_API __declspec(dllexport)
    #else
        #define TOYBOX_API __declspec(dllimport)
    #endif
#else
    #define TOYBOX_API
#endif