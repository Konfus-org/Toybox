#pragma once

namespace Toybox::Modules
{
    class ModuleServer
    {
    public:
        static const ModuleServer* GetInstance();

        ModuleServer();
        ~ModuleServer();

        template<class Interface>
        Interface* GetModule() const;
    };
}

