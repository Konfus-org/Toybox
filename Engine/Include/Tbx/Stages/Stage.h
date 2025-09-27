#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Represents a collection of toys.
    /// </summary>
    class TBX_EXPORT Stage
    {
    public:
        /// <summary>
        /// Creates a new instance of <see cref="Stage"/> with a root toy.
        /// </summary>
        Stage();
        ~Stage();

        /// <summary>
        /// Gets the root toy of the hierarchy.
        /// </summary>
        /// <returns>The root toy.</returns>
        Ref<Toy> GetRoot() const;

        /// <summary>
        /// Updates the toy hierarchy.
        /// </summary>
        void Update();

    private:
        Ref<Toy> _root = nullptr;
    };
}
