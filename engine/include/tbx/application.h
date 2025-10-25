#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/events/event.h"
#include "tbx/commands/command.h"
#include "tbx/dispatch/dispatcher.h"
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
        // Request exit from the main loop
        void request_exit();

        // Access message dispatcher
        MessageDispatcher& get_dispatcher() { return _dispatcher; }
        const MessageDispatcher& get_dispatcher() const { return _dispatcher; }

    private:
        void initialize();
        void update();
        void shutdown();

    private:
        AppDescription _desc = {};
        std::vector<LoadedPlugin> _loaded = {};
        MessageDispatcher _dispatcher;
        bool _should_exit = false;
    };
}
