#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/messages/coordinator.h"
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

    // Host application coordinating plugin lifecycle and message dispatching.
    // Thread-safety: The application and coordinator are intended to be used
    // on a single thread (the main loop). No internal synchronization is
    // provided.
    class Application
    {
    public:
        Application(const AppDescription& desc);
        ~Application();

        // Starts the application main loop. Returns process exit code.
        int run();
        // Request exit from the main loop
        void request_exit();

        // Access message dispatcher interface
        IMessageDispatcher& get_dispatcher() { return _dispatcher; }
        const IMessageDispatcher& get_dispatcher() const { return _dispatcher; }

    private:
        void initialize();
        void update();
        void shutdown();

    private:
        AppDescription _desc = {};
        // Loaded plugins (owning their instances and module handles).
        std::vector<LoadedPlugin> _loaded = {};
        // Concrete coordinator implementing dispatch + processing.
        MessageCoordinator _dispatcher;
        bool _should_exit = false;
    };
}
