#pragma once
#include "tbx/app/app_description.h"
#include "tbx/app/app_message_coordinator.h"
#include "tbx/app/window.h"
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
        IMessageDispatcher& get_dispatcher();

      private:
        void initialize();
        void update(DeltaTimer timer);
        void shutdown();
        void recieve_message(const Message& msg);

        AppDescription _desc;
        std::vector<LoadedPlugin> _loaded = {};
        AppMessageCoordinator _msg_coordinator = {};
        Window _main_window = {
            _msg_coordinator,
            _desc.name.empty() ? "Toybox Application" : _desc.name,
            {1280, 720},
            WindowMode::Windowed,
            false};
        bool _should_exit = false;
    };
}
