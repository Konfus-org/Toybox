#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    struct AppDescription
    {
        std::string name = "";
        std::filesystem::path working_root = "";
        std::filesystem::path assets_directory = "";
        std::filesystem::path plugins_directory = "";
        std::vector<std::string> requested_plugins = {};
    };
}
