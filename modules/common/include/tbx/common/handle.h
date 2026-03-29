#pragma once
#include "tbx/common/uuid.h"
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a general handle that references assets by name or UUID.
    /// @details
    /// Ownership: Stores owned name strings and UUID values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.

    struct Handle
    {
        Handle() = default;
        Handle(std::string asset_name)
        {
            const auto hasher = std::hash<std::string_view>();
            const auto hashed = static_cast<uint32>(hasher(asset_name));
            id = hashed == 0U ? Uuid(1U) : Uuid(hashed);
            name = std::move(asset_name);
        }
        Handle(Uuid asset_id)
            : id(asset_id)
        {
        }

        /// @brief
        /// Purpose: Returns true when the handle has either a name or UUID.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call concurrently.

        bool is_valid() const
        {
            return id.is_valid();
        }

        std::string name;
        Uuid id = {};
    };
}
