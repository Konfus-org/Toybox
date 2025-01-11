#pragma once
#include "TbxPCH.h"

struct PluginInfo
{
public:
    const std::string Name;
    const std::string Author;
    const std::string Version;
    const std::string Description;

    std::string ToString() const
    {
        return std::format("Name: {}\nAuthor: {}\nVersion: {}\nDescription: {}", Name, Author, Version, Description);
    }
};
