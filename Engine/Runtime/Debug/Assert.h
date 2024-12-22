#pragma once

// Basic assert defined in core, redefine it here to use log
#undef TBX_ASSERT

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(check, msg, ...) if(!(check)) TBX_CRITICAL(msg, __VA_ARGS__); if(!(check)) __debugbreak()
#else
    #define TBX_ASSERT(...)
#endif