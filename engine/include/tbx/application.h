#pragma once
#include "tbx/app_description.h"
#include "tbx/messages/coordinator.h"
#include "tbx/plugin_api/loaded_plugin.h"
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

       private:
        void initialize();
        void update(DeltaTimer timer);
        void shutdown();
        void handle_message(const Message& msg);

       private:
        AppDescription _desc = {};
        std::vector<LoadedPlugin> _loaded = {};
        MessageCoordinator _msg_coordinator;
        bool _should_exit = false;
    };
}
