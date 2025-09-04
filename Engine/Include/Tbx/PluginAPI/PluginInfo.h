#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Files/WorkingDirectory.h"
#include <string>
#include <vector>

namespace Tbx
{
    struct PluginInfo
    {
    public:
        EXPORT PluginInfo() = default;
        EXPORT PluginInfo(const std::string& pathToPluginFolder, const std::string& pluginFileName);

        EXPORT bool IsValid() const { return !(_name.empty() || _version.empty() || _lib.empty()); }
        EXPORT const std::string& GetName() const { return _name; }
        EXPORT const std::string& GetAuthor() const { return _author; }
        EXPORT const std::string& GetVersion() const { return _version; }
        EXPORT const std::string& GetDescription() const { return _description; }
        EXPORT const std::string& GetLib() const { return _lib; }
        EXPORT const std::string GetPathToLib() const { return Tbx::FileSystem::GetWorkingDirectory() + "\\" + GetLib(); }
        EXPORT const std::vector<std::string>& GetImplementedTypes() const { return _implementedTypes; }
        EXPORT const std::vector<std::string>& GetDependencies() const { return _dependencies; }

        EXPORT std::string ToString() const;

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
