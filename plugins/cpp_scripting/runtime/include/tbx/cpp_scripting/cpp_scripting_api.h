#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #ifdef TBX_SHARED_LIB
        #ifdef TBX_CPP_SCRIPTING_EXPORTING_SYMBOLS
        /// @brief Exports the C++ scripting runtime's public declarations while building the library.
            #define TBX_CPP_SCRIPTING_API __declspec(dllexport)
        #else
        /// @brief Imports the C++ scripting runtime's public declarations while consuming the library.
            #define TBX_CPP_SCRIPTING_API __declspec(dllimport)
        #endif
    #else
        #define TBX_CPP_SCRIPTING_API
    #endif
#else
    #define TBX_CPP_SCRIPTING_API
#endif
