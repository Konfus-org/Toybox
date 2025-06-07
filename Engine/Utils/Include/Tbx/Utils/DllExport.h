#pragma once

#ifdef TBX_PLATFORM_WINDOWS
    #ifdef COMPILING_TOYBOX
        /// <summary>
        /// This macro is used to export functions and classes from a DLL.
        /// You want this macro defined when you are creating a DLL.
        /// </summary>
        #define EXPORT __declspec(dllexport)
    #else
        /// <summary>
        /// This macro is used to import functions and classes from a DLL.
        /// If you are expecting export functionality ensure the TOOLBOX macro is defined.
        /// </summary>
        #define EXPORT __declspec(dllimport)
    #endif
#else
    #define EXPORT
#endif