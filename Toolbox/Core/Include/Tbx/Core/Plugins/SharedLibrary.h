#pragma once
#include <string>
#include <any>

namespace Tbx
{
    // A symbol can be anything...
    using Symbol = void*;

    class SharedLibrary 
    {
    public:
        bool Load(const std::string& path);
        void Unload();

        bool IsValid();

        std::string GetPath() const { return _path; }
        Symbol GetSymbol(const std::string& symbolName);
        void ListSymbols();

    private:
        std::string _path;
        std::any _handle = nullptr;
    };
}