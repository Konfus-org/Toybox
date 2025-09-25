#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Stages/Toy.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// Represents a collection of toys.
    /// </summary>
    class TBX_EXPORT Stage : std::enable_shared_from_this<Stage>
    {
    public:
        /// <summary>
        /// Creates a new instance of <see cref="Stage"/> with a root toy.
        /// </summary>
        Stage(Ref<EventBus> eventBus);

        /// <summary>
        /// Gets the root toy of the hierarchy.
        /// </summary>
        /// <returns>The root toy.</returns>
        Ref<Toy> GetRoot() const;

        /// <summary>
        /// Updates the toy hierarchy.
        /// </summary>
        void Update();

        /// <summary>
        /// Opens the stage.
        /// </summary>
        void Open();

        /// <summary>
        /// Closes the stage.
        /// </summary>
        void Close();

    private:
        Ref<Toy> _root = nullptr;
        Ref<EventBus> _eventBus = nullptr;
    };
}
