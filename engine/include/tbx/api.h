#pragma once
// TBX_DLL_EXPORT marks every public engine symbol. Static builds compile it away; with
// -DTBX_BUILD_SHARED=ON the engine builds as a dll (TBX_EXPORTS while building tbx,
// consumers import).
#if defined(TBX_SHARED)
    #if defined(_WIN32)
        #if defined(TBX_EXPORTS)
            #define TBX_DLL_EXPORT __declspec(dllexport)
        #else
            #define TBX_DLL_EXPORT __declspec(dllimport)
        #endif
    #else
        #define TBX_DLL_EXPORT __attribute__((visibility("default")))
    #endif
    #if defined(_MSC_VER)
        // Exported classes holding std members trip C4251/C4275; the contract here is "same
        // compiler, same CRT" — which a Toybox game build requires anyway.
        #pragma warning(disable : 4251 4275)
    #endif
#else
    #define TBX_DLL_EXPORT
#endif
