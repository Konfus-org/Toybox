#pragma once
#include "SharedLibrary.h"

namespace Toybox
{
    class LoadedModule
    {
    public:
        explicit(false) LoadedModule(const std::string& location);
        virtual ~LoadedModule();


        std::weak_ptr<Module> GetModule() const;
        std::weak_ptr<SharedLibrary> GetLibrary() const;

    private:
        void Load(const std::string& location);
        void Unload();

        std::shared_ptr<Module> _module = nullptr;
        std::shared_ptr<SharedLibrary> _library = nullptr;
    };
}
