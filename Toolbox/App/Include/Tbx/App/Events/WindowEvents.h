#pragma once
#include "Tbx/App/Windowing/IWindow.h"
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Events/Event.h>
#include <Tbx/Core/Math/Size.h>
#include <Tbx/Core/Ids/UID.h>
#include <string>
#include <memory>

namespace Tbx
{
    class EXPORT WindowEvent : public Event
    {
    public:
        int GetCategorization() const final
        { 
            return static_cast<int>(EventCategory::Window);
        }
    };

    class EXPORT WindowActionEvent : public WindowEvent
    {
    public:
        explicit WindowActionEvent(UID windowId) : _windowId(windowId) {}

        UID GetWindowId() const { return _windowId; }

    private:
        UID _windowId = -1;
    };

    class OpenNewWindowRequestEvent : public WindowEvent
    {
    public:
        EXPORT OpenNewWindowRequestEvent(const std::string& name, const WindowMode& mode, const Size& size)
            : _name(name), _mode(mode), _size(size) {}

        EXPORT const std::string& GetName() const { return _name; }
        EXPORT const WindowMode& GetMode() const { return _mode; }
        EXPORT const Size& GetSize() const { return _size; }

        EXPORT std::shared_ptr<IWindow> GetResult() const { return _result; }
        EXPORT void SetResult(const std::shared_ptr<IWindow>& window) { _result = window; }

        std::string ToString() const final
        {
            return "Create Window Event";
        }

    private:
        std::shared_ptr<IWindow> _result;
        std::string _name;
        WindowMode _mode;
        Size _size;
    };

    class EXPORT WindowFocusChangedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusChangedEvent(UID windowId, bool isFocused)
            : WindowActionEvent(windowId), _isFocused(isFocused) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }

        bool IsFocused() const { return _isFocused; }

    private:
        bool _isFocused = false;
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(UID windowId) 
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Close Event";
        }
    };

    class EXPORT WindowResizedEvent : public WindowActionEvent
    {
    public:
        WindowResizedEvent(UID windowId, uint width, uint height)
            : WindowActionEvent(windowId), _width(width), _height(height) {}

        Size GetSize() const
        {
            return Size(_width, _height);
        }

        std::string ToString() const final
        {
            return "Window Resize Event";
        }

    private:
        uint _width;
        uint _height;
    };
}