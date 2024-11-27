#pragma once
#include "DynamicLibrary.h"
#include "ModuleAPI.h"

namespace Toybox::Modules
{
    class LoadedModule
    {
    public:
        LoadedModule(const std::string& location);
        ~LoadedModule();

        const DynamicLibrary* GetLib() const;
        const Module* GetModule() const;

    private:
        DynamicLibrary* _library;
        Module* _module;
    };
}
