#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/ECS/Box.h"

namespace Tbx
{
    /// <summary>
    /// A play space is a collection of boxes.
    /// A play space is a way to group boxes together to make some "game world".
    /// It can be used to represent a level, a scene, or world.
    /// </summary>
    class EXPORT Playspace
    {
    public:
        /// <summary>
        /// Gets all the boxes in the play space.
        /// </summary>
        const std::vector<std::shared_ptr<Box>>& GetBoxes() const
        {
            return _boxes;
        }

        /// <summary>
        /// Adds a box to the play space.
        /// </summary>
        void AddBox(const std::shared_ptr<Box>& box)
        {
            _boxes.push_back(box);
        }

    private:
        std::vector<std::shared_ptr<Box>> _boxes;
    };
}
