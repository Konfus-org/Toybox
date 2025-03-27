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
        TBX_API static UID OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);

        TBX_API static std::weak_ptr<IWindow> GetMainWindow();
        TBX_API static std::weak_ptr<IWindow> GetWindow(const UID& id);
        TBX_API static std::vector<std::weak_ptr<IWindow>> GetAllWindows();

        TBX_API static void CloseWindow(const UID& id);
        TBX_API static void CloseAllWindows();

    private:
        static std::map<UID, std::shared_ptr<IWindow>> _windows;
        static UID _maindWindowId;
    };
}
