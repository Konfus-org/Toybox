#pragma once
#include "Tbx/Windowing/IWindow.h"
#include <vector>

namespace Tbx
{
    class WindowStack
    {
    public:
        EXPORT ~WindowStack();

        /// <summary>
        /// Operator to allow indexing into the window stack.
        /// </summary>
        EXPORT std::shared_ptr<IWindow> operator[](int index) const { return _windows[index]; }

        /// <summary>
        /// Adds an existing window to the stack
        /// </summary>
        /// <param name="window"></param>
        EXPORT void Push(std::shared_ptr<IWindow> window);

        EXPORT bool Contains(const Uid& id) const;
        EXPORT std::shared_ptr<IWindow> Get(const Uid& id) const;
        EXPORT const std::vector<std::shared_ptr<IWindow>>& GetAll() const;

        EXPORT void Remove(const Uid& id);
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
        std::vector<std::shared_ptr<IWindow>> _windows = {};
    };
}
