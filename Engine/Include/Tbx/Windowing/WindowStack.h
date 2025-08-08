#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Plugin API/PluginInterfaces.h"
#include <map>
#include <vector>

namespace Tbx
{
    class WindowStack
    {
    public:
        EXPORT WindowStack();
        EXPORT ~WindowStack();

        /// <summary>
        /// Adds an existing window to the stack
        /// </summary>
        /// <param name="window"></param>
        EXPORT void Push(std::shared_ptr<IWindow> window);

        /// <summary>
        /// Adds a new window to the sack and and opens it.
        /// </summary>
        /// <returns>The id of the newly created and opened window</returns>
        EXPORT UID Emplace(const std::string& name, const Size& size, const WindowMode& mode);

        EXPORT bool Contains(const UID& id) const;
        EXPORT std::shared_ptr<IWindow> Get(const UID& id) const;
        EXPORT const std::vector<std::shared_ptr<IWindow>>& GetAll();

        EXPORT void Remove(const UID& id);
        EXPORT void Clear();

        EXPORT std::vector<std::shared_ptr<IWindow>>::iterator begin() { return _windows.begin(); }
        EXPORT std::vector<std::shared_ptr<IWindow>>::iterator end() { return _windows.end(); }
        EXPORT std::vector<std::shared_ptr<IWindow>>::reverse_iterator rbegin() { return _windows.rbegin(); }
        EXPORT std::vector<std::shared_ptr<IWindow>>::reverse_iterator rend() { return _windows.rend(); }

        EXPORT std::vector<std::shared_ptr<IWindow>>::const_iterator begin() const { return _windows.begin(); }
        EXPORT std::vector<std::shared_ptr<IWindow>>::const_iterator end() const { return _windows.end(); }
        EXPORT std::vector<std::shared_ptr<IWindow>>::const_reverse_iterator rbegin() const { return _windows.rbegin(); }
        EXPORT std::vector<std::shared_ptr<IWindow>>::const_reverse_iterator rend() const { return _windows.rend(); }

    private:
        std::shared_ptr<IWindowFactoryPlugin> _windowFactory = nullptr;
        std::vector<std::shared_ptr<IWindow>> _windows = {};
    };
}
