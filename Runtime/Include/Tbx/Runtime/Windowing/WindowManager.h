#pragma once
#include "Tbx/Runtime/Layers/Layer.h"
#include "Tbx/Runtime/Windowing/IWindow.h"
#include "Tbx/Runtime/Events/WindowEvents.h"
#include "Tbx/Runtime/Events/ApplicationEvents.h"
#include <map>
#include <vector>

namespace Tbx
{
    class WindowManager : public Layer
    {
    public:
        WindowManager(const std::string_view& name) : Layer(name) {}
        ~WindowManager() override = default;

        bool IsOverlay() override;
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

        /// <summary>
        /// Creates and opens a new window, the first window will be set as the main window.
        /// </summary>
        /// <returns>The id of the newly created and opened window</returns>
        EXPORT static UID OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);

        EXPORT static std::weak_ptr<IWindow> GetMainWindow();
        EXPORT static std::weak_ptr<IWindow> GetFocusedWindow();

        EXPORT static std::weak_ptr<IWindow> GetWindow(const UID& id);
        EXPORT static std::vector<std::weak_ptr<IWindow>> GetAllWindows();

        EXPORT static void CloseWindow(const UID& id);
        EXPORT static void CloseAllWindows();

    private:
        static void OnWindowClose(const WindowClosedEvent& event);
        static void OnWindowFocusChanged(const WindowFocusChangedEvent& event);

        static std::map<UID, std::shared_ptr<IWindow>> _windows;
        static std::vector<UID> _windowsToCloseOnNextUpdate;

        static UID _mainWindowId;
        static UID _focusedWindowId;
        static UID _appUpdatedEventId;
        static UID _windowCloseEventId;
        static UID _windowFocusChangedEventId;
    };
}
