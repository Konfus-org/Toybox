#pragma once

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(check, msg) if(!(check)) TBX_CRITICAL(msg); if(!(check)) __debugbreak()
#else
    #define TBX_ASSERT(...)
#endif