#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TSS/Toy.h"
#include "Tbx/Events/EventBus.h"
#include <memory>
#include "Tbx/Memory/Refs/Refs.h"

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
        EXPORT Stage(Tbx::Ref<EventBus> eventBus);

        /// <summary>
        /// Gets the root toy of the hierarchy.
        /// </summary>
        /// <returns>The root toy.</returns>
        EXPORT Tbx::Ref<Toy> GetRoot() const;

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
        Tbx::Ref<Toy> _root = nullptr;
        Tbx::Ref<EventBus> _eventBus = nullptr;
    };
}
