#pragma once
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
    class Application : public IMessageHandler
    {
    public:
        Application(const AppDescription& desc);
        ~Application() override;

        // Starts the application main loop. Returns process exit code.
        int run();

        void on_message(const Message& msg) override;

    private:
        void initialize();
        void update(DeltaTimer timer);
        void shutdown();

    private:
        AppDescription _desc = {};
        std::vector<LoadedPlugin> _loaded = {};
        MessageCoordinator _msg_coordinator;
        bool _should_exit = false;
    };
}
