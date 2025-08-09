#pragma once
#include "Tbx/DllExport.h"
#include <string>
#include <any>

namespace Tbx
{
    // A symbol can be anything...
    using Symbol = void*;
#if defined(TBX_PLATFORM_WINDOWS)
    #include <windows.h>
    using LibHandle = HMODULE;
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_OSX)
    #include <dlfcn.h>
    using LibHandle = void*;
#else
    #error Unsupported platform
#endif

    class SharedLibrary 
    {
    public:
        EXPORT bool Load(const std::string& path);
        EXPORT void Unload();

        EXPORT bool IsValid();

        EXPORT std::string GetPath() const { return _path; }
        EXPORT Symbol GetSymbol(const std::string& symbolName);
        EXPORT void ListSymbols();

    private:
        std::string _path = "";
        LibHandle _handle = nullptr;
    };
}