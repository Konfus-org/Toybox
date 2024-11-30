#pragma once
#include "tbxpch.h"
#include "ModuleAPI.h"

namespace Toybox::Modules
{
    class ModuleServer
    {
    public:
        static const ModuleServer* GetInstance();

        ModuleServer();
        ~ModuleServer();

        Module* GetModule(const std::string& name) const;
    };
}

