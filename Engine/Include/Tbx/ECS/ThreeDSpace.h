#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/ECS/Toy.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// Represents a 3D space in which toys can be placed.
    /// </summary>
    class ThreeDSpace
    {
    public:
        /// <summary>
        /// Initializes a new instance of <see cref="World"/> with a root toy.
        /// </summary>
        EXPORT ThreeDSpace();

        /// <summary>
        /// Gets the root toy of the hierarchy.
        /// </summary>
        /// <returns>The root toy.</returns>
        EXPORT std::shared_ptr<Toy> GetRoot() const;

        /// <summary>
        /// Updates the toy hierarchy.
        /// </summary>
        EXPORT void Update();

    private:
        std::shared_ptr<Toy> _root = {};
    };
}
