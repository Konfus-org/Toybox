#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/window_description.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Window is a lightweight wrapper around a platform specific implementation managed through
    // messages.
    // Ownership: Does not own the underlying platform window; keeps no platform state locally.
    // Thread-safety: Not thread-safe; expected to be used on the main/UI thread.
    class TBX_API Window
    {
      public:
        Window(
            IMessageDispatcher& dispatcher,
            const WindowDescription& description,
            bool open_on_creation = true);
        ~Window();

        const WindowDescription& get_description();
        void set_description(const WindowDescription& description);
        bool is_open() const;
        void open();
        void close();

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        bool _is_open = false;
        WindowDescription _description = {};
    };
}
