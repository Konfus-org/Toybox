#pragma once
#include "tbx/app_description.h"
#include "tbx/application_context.h"
#include "tbx/messages/coordinator.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/service_locator.h"
#include "tbx/time/delta_time.h"
#include <vector>

namespace tbx
{
    // Host application coordinating plugin lifecycle and message dispatching.
    // Thread-safety: The application and coordinator are intended to be used
    // on a single thread (the main loop). No internal synchronization is
    // provided.
    class TBX_API Application
    {
       public:
        Application(const AppDescription& desc);
        ~Application();

        // Starts the application main loop. Returns process exit code.
        int run();

        // Access to the central service locator.
        ServiceLocator& services() { return _services; }
        const ServiceLocator& services() const { return _services; }

        // Access to the global message dispatcher.
        MessageCoordinator& dispatcher() { return _msg_coordinator; }
        const MessageCoordinator& dispatcher() const { return _msg_coordinator; }

       private:
        void initialize();
        void update(DeltaTimer timer);
        void shutdown();
        void handle_message(const Message& msg);

       private:
        AppDescription _desc = {};
        std::vector<LoadedPlugin> _loaded = {};
        MessageCoordinator _msg_coordinator;
        ServiceLocator _services;
        ApplicationContext _context = {};
        bool _should_exit = false;
    };
}
