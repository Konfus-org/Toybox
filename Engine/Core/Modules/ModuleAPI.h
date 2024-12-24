#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #define TBX_MODULE_API __declspec(dllexport)
#else
    #define TBX_MODULE_API
#endif
