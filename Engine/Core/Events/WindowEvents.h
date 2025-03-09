#pragma once
#include "TbxAPI.h"
#include "Event.h"
#include "Windowing/IWindow.h"
#include "Math/Size.h"
#include "Util/UID.h"

namespace Tbx
{
    class TBX_API WindowEvent : public Event
    {
    public:
        int GetCategorization() const final
        { 
            return static_cast<int>(EventCategory::Window);
        }
    };

    class TBX_API WindowActionEvent : public WindowEvent
    {
    public:
        explicit WindowActionEvent(UID windowId) : _windowId(windowId) {}

        UID GetWindowId() const { return _windowId; }

    private:
        UID _windowId = -1;
    };

    class OpenNewWindowEvent : public WindowEvent
    {
    public:
        TBX_API OpenNewWindowEvent(const std::string& name, const WindowMode& mode, const Size& size)
            : _name(name), _mode(mode), _size(size) {}

        TBX_API const std::string& GetName() const { return _name; }
        TBX_API const WindowMode& GetMode() const { return _mode; }
        TBX_API const Size& GetSize() const { return _size; }

        UID GetResult() const { return _result; }
        void SetResult(const UID& window) { _result = window; }

        std::string ToString() const final
        {
            return "Create Window Event";
        }

    private:
        UID _result = -1;
        std::string _name;
        WindowMode _mode;
        Size _size;
    };

    class GetWindowEvent : public WindowActionEvent
    {
    public:
        TBX_API explicit GetWindowEvent(UID windowId)
            : WindowActionEvent(windowId) {}

        TBX_API std::weak_ptr<IWindow> GetResult() const { return _result; }
        TBX_API void SetResult(const std::weak_ptr<IWindow>& window) { _result = window; }

        TBX_API std::string ToString() const final
        {
            return "Get Window Event";
        }

    private:
        std::weak_ptr<IWindow> _result;
    };

    class TBX_API WindowCloseEvent : public WindowActionEvent
    {
    public:
        std::string ToString() const final
        {
            return "Window Close Event";
        }
    };

    class TBX_API WindowResizeEvent : public WindowActionEvent
    {
    public:
        WindowResizeEvent(UID windowId, uint width, uint height)
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