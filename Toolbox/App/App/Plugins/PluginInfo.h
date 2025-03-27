#pragma once
#include "App/Plugins/PluginMetaReader.h"

namespace Tbx
{
    struct PluginInfo
    {
    public:
        void Load(const std::string& location)
        {
            auto metaData = PluginMetaReader::Read(location);
            if (metaData.empty()) return;

            _name = metaData["name"];
            _author = metaData["author"];
            _version = metaData["version"];
            _description = metaData["description"];
        }

        bool IsValid() const { return !(_name.empty() || _author.empty() || _version.empty() || _description.empty()); }
        std::string GetName() const { return _name; }
        std::string GetAuthor() const { return _author; }
        std::string GetVersion() const { return _version; }
        std::string GetDescription() const { return _description; }

        std::string ToString() const
        {
            return std::format("Name: {}\nAuthor: {}\nVersion: {}\nDescription: {}", _name, _author, _version, _description);
        }

    private:
        std::string _name;
        std::string _author;
        std::string _version;
        std::string _description;

    };
}
