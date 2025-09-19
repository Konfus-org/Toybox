#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventBus.h"
#include <vector>

namespace Tbx
{
    class WindowManager
    {
    public:
        EXPORT WindowManager(
            std::shared_ptr<IWindowFactory> windowFactory,
            std::shared_ptr<EventBus> eventBus);
        EXPORT ~WindowManager();

        EXPORT void UpdateWindows();

        EXPORT std::shared_ptr<IWindow> GetMainWindow() const;
        EXPORT const std::vector<std::shared_ptr<IWindow>>& GetAllWindows() const;
        EXPORT std::shared_ptr<IWindow> GetWindow(const Uid& id) const;

        EXPORT Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080));

        EXPORT void CloseWindow(const Uid& id);
        EXPORT void CloseAllWindows();

    private:
        std::shared_ptr<IWindowFactory> _windowFactory = {};
        std::shared_ptr<EventBus> _eventBus = {};
        Uid _mainWindowId = Uid::Invalid;
        WindowStack _stack = {};
    };
}
