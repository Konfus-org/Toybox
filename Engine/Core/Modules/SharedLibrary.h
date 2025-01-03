#pragma once
#include "TbxPCH.h"

namespace Tbx
{
    // A symbol can be anything...
    using Symbol = void*;

    class SharedLibrary 
    {
    public:
        explicit(false) SharedLibrary(const std::string& path);
        ~SharedLibrary();

        std::string GetPath() const;

        bool IsValid();

        Symbol GetSymbol(const std::string& symbolName);
        void ListSymbols();

    private:
        std::string _path;
        std::any _handle = nullptr;

        bool Load(const std::string& path);
        void Unload();
    };
}