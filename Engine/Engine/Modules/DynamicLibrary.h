#pragma once
#include "tbxpch.h"

namespace Toybox
{
    class DynamicLibrary 
    {
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary();

        std::string GetName() const;
        bool Load(const std::string path);
        void* GetSymbol(const std::string& name);
        void Unload();

    private:
        std::string _name;
        void* _handle = nullptr;
    };
}