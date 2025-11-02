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
        Fullscreen
    };

    struct TBX_API WindowDescription
    {
        math::Size size = { 1280, 720 };
        WindowMode mode = WindowMode::Windowed;
        std::string title = "Toybox";
    };

    using WindowImpl = void*;

    // Window is a lightweight wrapper around a platform specific implementation managed through
    // messages.
    class TBX_API Window
    {
       public:
        Window(
            IMessageDispatcher& dispatcher,
            WindowImpl implementation,
            const WindowDescription& description);

        const Uuid& get_id() const;
        const WindowDescription& get_description();
        void set_description(const WindowDescription& description);

        WindowImpl get_implementation() const;

       private:
        void apply_description_update(const WindowDescription& description);
        void set_implementation(WindowImpl implementation);

        IMessageDispatcher* _dispatcher = nullptr;
        WindowImpl _implementation = nullptr;
        WindowDescription _description = {};
        Uuid _id = Uuid::generate();
    };
}
