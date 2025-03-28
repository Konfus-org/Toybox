#pragma once
#include "IWindow.h"
#include <map>

namespace Tbx
{
    class WindowManager
    {
    public:
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
        static std::map<UID, std::shared_ptr<IWindow>> _windows;
        static UID _mainWindowId;
        static UID _focusedWindowId;
    };
}
