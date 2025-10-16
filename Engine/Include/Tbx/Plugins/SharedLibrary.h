#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    // A symbol can be anything...
    using Symbol = void*;
#if defined(TBX_PLATFORM_WINDOWS)
    #include <Windows.h>
    using LibHandle = HMODULE;
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #include <dlfcn.h>
    using LibHandle = void*;
#else
    #error Unsupported platform
#endif

    class TBX_EXPORT SharedLibrary
    {
    public:
        SharedLibrary(const std::string& path);
        ~SharedLibrary();

        bool IsValid();
        std::string GetPath() const { return _path; }
        Symbol GetSymbol(const std::string& symbolName);
        void ListSymbols();

    private:
        std::string _path = "";
        LibHandle _handle = nullptr;
    };
}
