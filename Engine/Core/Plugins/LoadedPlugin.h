#pragma once
#include "SharedLibrary.h"
#include "Plugin.h"

namespace Tbx
{
    class LoadedPlugin
    {
    public:
        explicit(false) LoadedPlugin(const std::string& location);
        virtual ~LoadedPlugin();

        std::weak_ptr<Plugin> GetPlugin() const;
        std::weak_ptr<SharedLibrary> GetLibrary() const;

    private:
        void Load(const std::string& location);
        void Unload();

        std::shared_ptr<Plugin> _plugin = nullptr;
        std::shared_ptr<SharedLibrary> _library = nullptr;
    };
}
