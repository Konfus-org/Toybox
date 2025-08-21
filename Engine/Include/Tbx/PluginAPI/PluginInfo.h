#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    struct PluginInfo
    {
    public:
        EXPORT PluginInfo() = default;
        EXPORT PluginInfo(const std::string& pathToPluginFolder, const std::string& pluginFileName);

        EXPORT bool IsValid() const { return !(_name.empty() || _author.empty() || _version.empty() || _description.empty()); }
        EXPORT std::string GetName() const { return _name; }
        EXPORT std::string GetAuthor() const { return _author; }
        EXPORT std::string GetVersion() const { return _version; }
        EXPORT std::string GetDescription() const { return _description; }
        EXPORT std::string GetLib() const { return _lib; }
        EXPORT int GetPriority() const { return _priority; }
        EXPORT std::string GetFolderPath() const { return _pathToFolder; }
        EXPORT std::string GetFilePath() const { return GetFolderPath() + "/" + GetLib(); }

        EXPORT std::string ToString() const;

    private:
        void Load(const std::string& pathToPluginFile);

        std::string _name = "";
        std::string _author = "";
        std::string _version = "";
        std::string _description = "";
        std::string _lib = "";
        std::string _pathToFolder = "";
        int _priority = -2147483647;
    };
}
