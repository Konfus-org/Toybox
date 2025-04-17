#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/TBS/IBoxable.h"
#include "Tbx/Core/TBS/Toy.h"
#include <vector>
#include <memory>

namespace Tbx
{
    /// <summary>
    /// A box is a collection of toys or "game objects".
    /// A box is a way to group toys together. A good example of this is a prefab.
    /// A box can contain sub-boxes.
    /// </summary>
    struct Box : public IBoxable
    {
    public:
        /// <summary>
        /// Gets all the items in the box.
        /// </summary>
        EXPORT std::vector<std::shared_ptr<IBoxable>> GetAllItems() const
        {
            std::vector<std::shared_ptr<IBoxable>> items;
            items.reserve(_toys.size() + _boxes.size());
            for (const auto& toy : _toys)
            {
                items.push_back(toy);
            }
            for (const auto& box : _boxes)
            {
                items.push_back(box);
            }
            return items;
        }

        /// <summary>
        /// Adds an item to the box.
        /// </summary>
        EXPORT void AddItem(const std::shared_ptr<IBoxable>& item)
        {
            if (std::dynamic_pointer_cast<Toy>(item))
            {
                _toys.push_back(std::dynamic_pointer_cast<Toy>(item));
            }
            else if (std::dynamic_pointer_cast<Box>(item))
            {
                _boxes.push_back(std::dynamic_pointer_cast<Box>(item));
            }
        }

        /// <summary>
        /// Adds a toy to the box.
        /// </summary>
        EXPORT void AddToy(const std::shared_ptr<Toy>& toy)
        {
            _toys.push_back(toy);
        }

        /// <summary>
        /// Creates and adds a toy to the box then returns it.
        /// </summary>
        EXPORT std::shared_ptr<Toy> AddToy()
        {
            return _toys.emplace_back();
        }

        /// <summary>
        /// Adds a box to the box.
        /// </summary>
        EXPORT void AddBox(const std::shared_ptr<Box>& box)
        {
            _boxes.push_back(box);
        }

        /// <summary>
        /// Creates and adds a box to the box then returns it.
        /// </summary>
        EXPORT std::shared_ptr<Box> AddBox()
        {
            return _boxes.emplace_back();
        }

    private:
        std::vector<std::shared_ptr<Toy>> _toys;
        std::vector<std::shared_ptr<Box>> _boxes;
    };
}