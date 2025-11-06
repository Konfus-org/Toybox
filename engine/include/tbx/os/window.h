#pragma once
#include "tbx/ids/uuid.h"
#include "tbx/math/size.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    enum class TBX_API WindowMode
    {
        Windowed,
        Borderless,
        Fullscreen,
        Minimized
    };

    struct TBX_API WindowDescription
    {
        math::Size size = {1280, 720};
        WindowMode mode = WindowMode::Windowed;
        std::string title = "Toybox";
        Uuid id = Uuid::generate();
    };

    // Window is a lightweight wrapper around a platform specific implementation managed through
    // messages.
    // Ownership: Does not own the underlying platform window; stores an opaque handle.
    // Thread-safety: Not thread-safe; expected to be used on the main/UI thread.
    class TBX_API Window
    {
      public:
        using WindowImpl = void*;

        Window(
            IMessageDispatcher& dispatcher,
            const WindowDescription& description,
            bool open_on_creation = true);
        ~Window();

        const WindowDescription& get_description();
        void set_description(const WindowDescription& description);
        bool is_open();
        void open();
        void close();

        template <typename T>
        WindowImpl get_implementation() const
        {
            return static_cast<T>(_implementation);
        }

      private:
        void apply_description_update(const WindowDescription& description);

        IMessageDispatcher* _dispatcher = nullptr;
        WindowImpl _implementation = nullptr;
        WindowDescription _description = {};
    };
}
