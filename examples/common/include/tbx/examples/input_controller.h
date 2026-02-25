#pragma once
#include "tbx/input/input_manager.h"
#include <string>

namespace tbx::examples
{
    /// <summary>
    /// Purpose: Shares input-manager scheme binding and lookup behavior for runtime controllers.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores non-owning pointers/references to InputManager-owned scheme state.
    /// Thread Safety: Not thread-safe; intended for main-thread ECS/input update usage.
    /// </remarks>
    class InputController
    {
      public:
        /// <summary>
        /// Purpose: Enables safe polymorphic destruction for shared input-controller bases.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own external resources.
        /// Thread Safety: Main-thread lifecycle usage.
        /// </remarks>
        virtual ~InputController() = default;

      protected:
        /// <summary>
        /// Purpose: Records the manager and scheme name used for later scheme lookups.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not own InputManager; caller keeps manager alive.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        void bind_input_context(InputManager& input_manager, const std::string& scheme_name);

        /// <summary>
        /// Purpose: Clears stored manager/scheme references.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases non-owning references only.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        void clear_input_context();

        /// <summary>
        /// Purpose: Returns the currently tracked input scheme name.
        /// </summary>
        /// <remarks>
        /// Ownership: Returned reference aliases controller-owned string storage.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        const std::string& get_input_scheme_name() const;

        /// <summary>
        /// Purpose: Resolves the currently tracked scheme from InputManager storage.
        /// </summary>
        /// <remarks>
        /// Ownership: Returned reference is non-owning and owned by InputManager.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        InputScheme& get_registered_input_scheme() const;

      private:
        InputManager* _input_manager = nullptr;
        std::string _scheme_name = "";
    };
}
