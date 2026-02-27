#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #ifdef TBX_SHARED_LIB
        #ifdef TBX_PLUGIN_EXPORTING_SYMBOLS
            /// <summary>
            /// Purpose: Marks plugin public types for symbol export while building a plugin shared
            /// library.
            /// Ownership: Preprocessor macro with no ownership semantics.
            /// Thread Safety: Compile-time only.
            /// </summary>
            #define TBX_PLUGIN_API __declspec(dllexport)
        #else
            /// <summary>
            /// Purpose: Marks plugin public types for symbol import while consuming a plugin shared
            /// library.
            /// Ownership: Preprocessor macro with no ownership semantics.
            /// Thread Safety: Compile-time only.
            /// </summary>
            #define TBX_PLUGIN_API __declspec(dllimport)
        #endif
    #else
        #define TBX_PLUGIN_API
    #endif
#else
    #define TBX_PLUGIN_API
#endif
