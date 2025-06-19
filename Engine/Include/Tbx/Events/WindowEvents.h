#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/Math/Size.h"
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

    class EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(UID windowId)
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(UID windowId) 
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };

    class EXPORT WindowResizedEvent : public WindowActionEvent
    {
    public:
        WindowResizedEvent(UID windowId, uint width, uint height)
            : WindowActionEvent(windowId), _width(width), _height(height) {}

        Size GetNewSize() const
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

    class OpenNewWindowRequest : public WindowEvent
    {
    public:
        EXPORT OpenNewWindowRequest(const std::string& name, const WindowMode& mode, const Size& size)
            : _name(name), _mode(mode), _size(size) {
        }

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
}