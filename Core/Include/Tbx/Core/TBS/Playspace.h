#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/TBS/Box.h"

namespace Tbx
{
    /// <summary>
    /// A play space is a collection of boxes.
    /// A play space is a way to group boxes together to make some "game world".
    /// It can be used to represent a level, a scene, or world.
    /// </summary>
    class Playspace
    {
    public:
        /// <summary>
        /// Gets all the boxes in the play space.
        /// </summary>
        EXPORT const std::vector<std::shared_ptr<Box>>& GetBoxes() const
        {
            return _boxes;
        }

        /// <summary>
        /// Adds a box to the play space.
        /// </summary>
        EXPORT void AddBox(const std::shared_ptr<Box>& box)
        {
            _boxes.push_back(box);
        }

        /// <summary>
        /// Creates and adds a box to the play space then returns it.
        /// </summary>
        EXPORT std::shared_ptr<Box> AddBox()
        {
            return _boxes.emplace_back(new Box());
        }

    private:
        std::vector<std::shared_ptr<Box>> _boxes = {};
    };
}
