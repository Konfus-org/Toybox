#pragma once
#include "Tbx/Runtime/Plugin Server/PluginMetaReader.h"
#include <Tbx/Core/DllExport.h>

namespace Tbx
{
    struct PluginInfo
    {
    public:
        EXPORT void Load(const std::string& location)
        {
            auto metaData = PluginMetaReader::Read(location);
            if (metaData.empty()) return;

            _name = metaData["name"];
            _author = metaData["author"];
            _version = metaData["version"];
            _description = metaData["description"];
            _lib = metaData["lib"];
        }

        EXPORT bool IsValid() const { return !(_name.empty() || _author.empty() || _version.empty() || _description.empty()); }
        EXPORT std::string GetName() const { return _name; }
        EXPORT std::string GetAuthor() const { return _author; }
        EXPORT std::string GetVersion() const { return _version; }
        EXPORT std::string GetDescription() const { return _description; }
        EXPORT std::string GetLib() const { return _lib; }

        EXPORT std::string ToString() const
        {
            return std::format("Name: {}\nAuthor: {}\nVersion: {}\nDescription: {}", _name, _author, _version, _description);
        }

    private:
        std::string _name;
        std::string _author;
        std::string _version;
        std::string _description;
        std::string _lib;
    };
}
