#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/ILoggable.h"
#include <string>
#include <format>

namespace Tbx
{
    struct PluginInfo : ILoggable
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
        EXPORT std::string GetLocation() const { return _pathToFolder; }

        EXPORT std::string ToString() const override
        {
            return std::format("Name: {}\nAuthor: {}\nVersion: {}\nDescription: {}", _name, _author, _version, _description);
        }

    private:
        void Load(const std::string& pathToPluginFile);

        std::string _name = "";
        std::string _author = "";
        std::string _version = "";
        std::string _description = "";
        std::string _lib = "";
        std::string _pathToFolder = "";
        int _priority = 2147483647;
    };
}
