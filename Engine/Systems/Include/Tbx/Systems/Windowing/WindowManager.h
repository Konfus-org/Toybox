#pragma once
#include "Tbx/Systems/Windowing/IWindow.h"
#include "Tbx/Systems/Windowing/WindowEvents.h"
#include <map>
#include <vector>

namespace Tbx
{
    class WindowManager
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Shutdown();
        EXPORT static void Update();

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
        static UID _windowCloseEventId;
        static UID _windowFocusChangedEventId;
    };
}
