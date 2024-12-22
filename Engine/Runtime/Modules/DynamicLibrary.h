#pragma once
#include "tbxpch.h"
#include <Core.h>

namespace Toybox
{
    // A symbol can be anything...
    using Symbol = void*;

    class DynamicLibrary 
    {
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary();

        std::string GetPath() const;

        bool Load(const std::string& path);
        void Unload();

        Symbol GetSymbol(const std::string& symbolName);
        void ListSymbols();

    private:
        std::string _path;
        std::any _handle = nullptr;
    };
}