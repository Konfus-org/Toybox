#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Memory/Refs.h"
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
        EXPORT Ref<IWindow> operator[](int index) const { return _windows[index]; }

        /// <summary>
        /// Adds an existing window to the stack
        /// </summary>
        /// <param name="window"></param>
        EXPORT void Push(Ref<IWindow> window);

        EXPORT bool Contains(const Uid& id) const;
        EXPORT Ref<IWindow> Get(const Uid& id) const;
        EXPORT const std::vector<Ref<IWindow>>& GetAll() const;

        EXPORT void Remove(const Uid& id);
        EXPORT void Clear();

        EXPORT std::vector<Ref<IWindow>>::iterator begin() { return _windows.begin(); }
        EXPORT std::vector<Ref<IWindow>>::iterator end() { return _windows.end(); }
        EXPORT std::vector<Ref<IWindow>>::reverse_iterator rbegin() { return _windows.rbegin(); }
        EXPORT std::vector<Ref<IWindow>>::reverse_iterator rend() { return _windows.rend(); }

        EXPORT std::vector<Ref<IWindow>>::const_iterator begin() const { return _windows.begin(); }
        EXPORT std::vector<Ref<IWindow>>::const_iterator end() const { return _windows.end(); }
        EXPORT std::vector<Ref<IWindow>>::const_reverse_iterator rbegin() const { return _windows.rbegin(); }
        EXPORT std::vector<Ref<IWindow>>::const_reverse_iterator rend() const { return _windows.rend(); }

    private:
        std::vector<Ref<IWindow>> _windows = {};
    };
}
