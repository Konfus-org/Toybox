#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/ECS/IBoxable.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// A box is a collection of toys or "game objects".
    /// A box is a way to group toys together. A good example of this is a prefab.
    /// A box can contain sub-boxes.
    /// </summary>
    struct EXPORT Box : public IBoxable
    {
    public:
        /// <summary>
        /// Gets all the items in the box.
        /// </summary>
        const std::vector<std::shared_ptr<IBoxable>>& GetAllItems() const
        {
            return _items;
        }

        /// <summary>
        /// Adds an item to the box.
        /// </summary>
        void AddItem(const std::shared_ptr<IBoxable>& item)
        {
            _items.push_back(item);
        }

    private:
        std::vector<std::shared_ptr<IBoxable>> _items;
    };
}