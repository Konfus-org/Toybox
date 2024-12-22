#pragma once
#include "Module.h"

#ifdef TBX_PLATFORM_WINDOWS
    #define TBX_MODULE_API __declspec(dllexport)
#else
    #define TBX_MODULE_API
#endif
