#pragma once
#include "Tbx/Windowing/Window.h"
#include "Tbx/Memory/Refs.h"
#include <vector>

namespace Tbx
{
    class TBX_EXPORT WindowStack
    {
    public:
        ~WindowStack();

        /// <summary>
        /// Operator to allow indexing into the window stack.
        /// </summary>
        Ref<Window> operator[](int index) const { return _windows[index]; }

        /// <summary>
        /// Adds an existing window to the stack
        /// </summary>
        /// <param name="window"></param>
        void Push(Ref<Window> window);

        bool Contains(const Uid& id) const;
        Ref<Window> Get(const Uid& id) const;
        const std::vector<Ref<Window>>& GetAll() const;

        void Remove(const Uid& id);
        void Clear();

        std::vector<Ref<Window>>::iterator begin() { return _windows.begin(); }
        std::vector<Ref<Window>>::iterator end() { return _windows.end(); }
        std::vector<Ref<Window>>::reverse_iterator rbegin() { return _windows.rbegin(); }
        std::vector<Ref<Window>>::reverse_iterator rend() { return _windows.rend(); }

        std::vector<Ref<Window>>::const_iterator begin() const { return _windows.begin(); }
        std::vector<Ref<Window>>::const_iterator end() const { return _windows.end(); }
        std::vector<Ref<Window>>::const_reverse_iterator rbegin() const { return _windows.rbegin(); }
        std::vector<Ref<Window>>::const_reverse_iterator rend() const { return _windows.rend(); }

    private:
        std::vector<Ref<Window>> _windows = {};
    };
}
