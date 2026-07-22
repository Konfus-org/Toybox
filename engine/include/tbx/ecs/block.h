#pragma once
#include "tbx/utils/api.h"

namespace tbx::ecs
{
    /// @brief
    /// Purpose: Base of every block type (data attachable to toys). Deriving from it is what
    /// makes a type a block: reflection::register_type detects the base and stamps the
    /// type-erased ecs accessors onto the TypeInfo — block types are exactly the reflected
    /// types whose block facet is set.
    struct TBX_API Block
    {
    };
}
