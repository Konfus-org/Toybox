#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #define TBX_API __declspec(dllexport)
#else
    #define TBX_API
#endif