#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventBus.h"
#include <vector>
#include "Tbx/TypeAliases/Pointers.h"

namespace Tbx
{
    class WindowManager
    {
    public:
        EXPORT WindowManager(
            Tbx::Ref<IWindowFactory> windowFactory,
            Tbx::Ref<EventBus> eventBus);
        EXPORT ~WindowManager();

        EXPORT void UpdateWindows();

        EXPORT Tbx::Ref<IWindow> GetMainWindow() const;
        EXPORT const std::vector<Tbx::Ref<IWindow>>& GetAllWindows() const;
        EXPORT Tbx::Ref<IWindow> GetWindow(const Uid& id) const;

        EXPORT Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080));

        EXPORT void CloseWindow(const Uid& id);
        EXPORT void CloseAllWindows();

    private:
        Tbx::Ref<IWindowFactory> _windowFactory = {};
        Tbx::Ref<EventBus> _eventBus = {};
        Uid _mainWindowId = Consts::Invalid::Uid;
        WindowStack _stack = {};
    };
}
