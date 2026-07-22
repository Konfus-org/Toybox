#pragma once
#include "tbx/api.h"
#include "tbx/utils/uuid.h"

namespace tbx
{
    /// @brief
    /// Purpose: Base of every block type (data attachable to toys). Deriving from it is what
    /// makes a type a block: reflection::register_type detects the base and stamps the
    /// type-erased ecs accessors onto the TypeInfo — block types are exactly the reflected
    /// types whose block facet is set.
    struct TBX_API Block
    {
        Uuid id = Uuid::generate();

        /// @brief
        /// Purpose: Whether this component is active. Disabled components are skipped by their
        /// systems.
        /// @details
        /// Ownership: Value type. Hidden from the property grid — the inspector exposes it as the
        /// toggle in the component header rather than as an ordinary row. Thread Safety: Safe to
        /// read concurrently; synchronize mutation externally.
        bool is_enabled = true;
    };
}
