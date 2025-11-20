#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/time/delta_time.h"
#include <vector>

namespace tbx
{
    // Host application coordinating plugin lifecycle and message dispatching.
    // Ownership: Owns loaded plugins and the message coordinator; callers do
    // not own any references returned by getters beyond their lifetimes.
    // Thread-safety: Intended for use on a single thread (the main loop).
    // No internal synchronization is provided.
    class TBX_API Application
    {
      public:
        Application(AppDescription desc);
        ~Application();

        // Starts the application main loop. Returns process exit code.
        int run();

        const AppDescription& get_description() const;

        IMessageDispatcher& get_dispatcher()
        {
            return static_cast<IMessageDispatcher&>(_msg_coordinator);
        }

      private:
        void initialize();
        void update(DeltaTimer timer);
        void shutdown();
        void handle_message(const Message& msg);

        const AppDescription _desc;
        std::vector<LoadedPlugin> _loaded = {};
        AppMessageCoordinator _msg_coordinator;
        bool _should_exit = false;
    };
}
