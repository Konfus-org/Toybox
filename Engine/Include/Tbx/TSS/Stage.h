#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TSS/Toy.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// Represents a collection of toys.
    /// </summary>
    class Stage : std::enable_shared_from_this<Stage>
    {
    public:
        /// <summary>
        /// Creates a new instance of <see cref="Stage"/> with a root toy.
        /// </summary>
        EXPORT Stage(Ref<EventBus> eventBus);

        /// <summary>
        /// Gets the root toy of the hierarchy.
        /// </summary>
        /// <returns>The root toy.</returns>
        EXPORT Ref<Toy> GetRoot() const;

        /// <summary>
        /// Updates the toy hierarchy.
        /// </summary>
        EXPORT void Update();

        /// <summary>
        /// Opens the stage.
        /// </summary>
        EXPORT void Open();

        /// <summary>
        /// Closes the stage.
        /// </summary>
        EXPORT void Close();

    private:
        Ref<Toy> _root = nullptr;
        Ref<EventBus> _eventBus = nullptr;
    };
}
