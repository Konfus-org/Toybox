#pragma once
#include <Tbx/App/App.h>
#include <string>
#include <vector>

namespace Tbx::Launcher
{
    struct AppConfig
    {
        std::string Name = "";
        AppSettings Settings = {};
        std::vector<std::string> Plugins = {};
        std::vector<std::string> Arguments = {};
    };

    AppStatus Launch(const AppConfig& config);
}
