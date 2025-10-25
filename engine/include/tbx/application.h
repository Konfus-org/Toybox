#pragma once
#include "tbx/memory/smart_pointers.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    class Plugin;

    struct AppDescription
    {
        std::string name = "";
        std::filesystem::path working_root = "";
        std::filesystem::path assets_directory = "";
        std::filesystem::path plugins_directory = "";
        std::vector<std::string> requested_plugins = {};
    };

    class Application
    {
    public:
        Application(const AppDescription& desc);
        ~Application();

        // Starts the application main loop. Returns process exit code.
        int run();

    private:
        void initialize();
        void update();
        void shutdown();

    private:
        AppDescription _desc = {};
        std::vector<Scope<Plugin>> _plugs = {};
    };
}
