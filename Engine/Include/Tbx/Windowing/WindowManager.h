#pragma once
#include "Tbx/Windowing/Window.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class TBX_EXPORT WindowManager
    {
    public:
        WindowManager() = default;
        WindowManager(
            Ref<IWindowFactory> windowFactory,
            Ref<EventBus> eventBus);
        ~WindowManager();

        void Update() const;

        Ref<Window> GetMainWindow() const;
        const std::vector<Ref<Window>>& GetAllWindows() const;
        Ref<Window> GetWindow(const Uid& id) const;

        Uid OpenWindow(const std::string& name, const WindowMode& mode, const Size& size = Size(1920, 1080));

        void CloseWindow(const Uid& id);
        void CloseAllWindows();

    private:
        Ref<IWindowFactory> _windowFactory = {};
        Ref<EventBus> _eventBus = {};
        Uid _mainWindowId = Uid::Invalid;
        WindowStack _stack = {};
    };
}
