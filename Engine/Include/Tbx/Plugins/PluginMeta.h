#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/DllExport.h"
#include "Tbx/Files/Paths.h"
#include <string>
#include <vector>

namespace Tbx
{
    struct TBX_EXPORT PluginMeta : public IPrintable
    {
    public:
        PluginMeta() = default;
        PluginMeta(const std::string& pathToPluginFolder, const std::string& pluginFileName);

        bool IsValid() const { return !(_name.empty() || _version.empty() || _lib.empty()); }
        const std::string& GetName() const { return _name; }
        const std::string& GetAuthor() const { return _author; }
        const std::string& GetVersion() const { return _version; }
        const std::string& GetDescription() const { return _description; }
        const std::string& GetLib() const { return _lib; }
        std::string GetPathToLib() const { return FileSystem::GetWorkingDirectory() + "/" + GetLib(); }
        const std::vector<std::string>& GetImplementedTypes() const { return _implementedTypes; }
        const std::vector<std::string>& GetDependencies() const { return _dependencies; }

        std::string ToString() const override;

    private:
        void Load(const std::string& pathToPluginFile);

        std::string _name = "";
        std::string _author = "";
        std::string _version = "";
        std::string _description = "";
        std::string _lib = "";
        std::vector<std::string> _implementedTypes = {};
        std::vector<std::string> _dependencies = {};
    };
}
