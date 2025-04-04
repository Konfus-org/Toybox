#pragma once
#include "Tbx/Core/DllExport.h"
#include <string>
#include <any>

namespace Tbx
{
    // A symbol can be anything...
    using Symbol = void*;

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
        std::any _handle = nullptr;
    };
}