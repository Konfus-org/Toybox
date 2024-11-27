#pragma once
#include <string>

namespace Toybox::Modules
{
    class DynamicLibrary {
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary();

        bool Load(const std::string& path);
        void* GetSymbol(const std::string& name);
        void Unload();

    private:
        void* _handle = nullptr;
    };
}