#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class WindowManager
    {
    public:
        EXPORT WindowManager(
            Ref<IWindowFactory> windowFactory,
            Ref<EventBus> eventBus);
        EXPORT ~WindowManager();

        EXPORT void UpdateWindows();

        EXPORT Ref<IWindow> GetMainWindow() const;
        EXPORT const std::vector<Ref<IWindow>>& GetAllWindows() const;
        EXPORT Ref<IWindow> GetWindow(const Uid& id) const;

        EXPORT Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080));

        EXPORT void CloseWindow(const Uid& id);
        EXPORT void CloseAllWindows();

    private:
        Ref<IWindowFactory> _windowFactory = {};
        Ref<EventBus> _eventBus = {};
        Uid _mainWindowId = Uid::Invalid;
        WindowStack _stack = {};
    };
}
