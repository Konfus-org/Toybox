#pragma once

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(check, msg, ...) if(!(check)) TBX_CRITICAL(msg, __VA_ARGS__); if(!(check)) __debugbreak()
#else
    #define TBX_ASSERT(...)
#endif