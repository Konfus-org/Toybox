#pragma once
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Events/EventBus.h"
#include <any>
#include <functional>

namespace Tbx
{
    using NativeHandle = size_t;
    using NativeWindow = std::any;

    enum class TBX_EXPORT WindowMode
    {
        Windowed,
        Fullscreen,
        Borderless,
        FullscreenBorderless
    };

    class TBX_EXPORT Window
    {
    public:
        virtual ~Window() = default;

        virtual void Open() = 0;
        virtual void Close() = 0;
        virtual void Update() = 0;
        virtual void Focus() = 0;

        virtual bool IsClosed() = 0;
        virtual bool IsFocused() = 0;

        virtual NativeHandle GetNativeHandle() const = 0;
        virtual NativeWindow GetNativeWindow() const = 0;

        virtual const std::string& GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual void SetMode(const WindowMode& mode) = 0;
        virtual Tbx::WindowMode GetMode() = 0;

        virtual const Size& GetSize() const = 0;
        virtual void SetSize(const Size& size) = 0;

    public:
        Uid Id = Uid::Generate();
    };

    struct WindowRefHasher
    {
        size_t operator()(const Ref<Window>& window) const noexcept
        {
            return std::hash<const Window*>()(window.get());
        }
    };

    struct WindowRefEqual
    {
        bool operator()(const Ref<Window>& lhs, const Ref<Window>& rhs) const noexcept
        {
            return lhs.get() == rhs.get();
        }
    };

    class TBX_EXPORT IWindowFactory
    {
    public:
        virtual ~IWindowFactory() = default;
        virtual Ref<Window> Create(
            const std::string& title,
            const Size& size,
            const WindowMode& mode,
            Ref<EventBus> eventBus) = 0;
    };
}
