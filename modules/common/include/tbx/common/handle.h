#pragma once
#include "tbx/common/uuid.h"
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents a general handle that references assets by name or UUID.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores owned name strings and UUID values.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct Handle
    {
        Handle() = default;

        /// <summary>
        /// Purpose: Constructs a handle from an asset name.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the provided name into the handle.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        Handle(const char* asset_name)
            : Handle(asset_name ? std::string(asset_name) : std::string())
        {
        }

        /// <summary>
        /// Purpose: Constructs a handle from an asset name.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the provided name into the handle.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        Handle(std::string asset_name)
            : name(std::move(asset_name))
            , id(hash_name(name))
        {
        }

        /// <summary>
        /// Purpose: Constructs a handle from an asset UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores the provided UUID value by copy.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        Handle(Uuid asset_id)
            : id(asset_id)
        {
        }

      private:
        static Uuid hash_name(std::string_view value)
        {
            if (value.empty())
            {
                return {};
            }
            const auto hasher = std::hash<std::string_view>();
            const auto hashed = static_cast<uint32>(hasher(value));
            return hashed == 0U ? Uuid(1U) : Uuid(hashed);
        }

      public:
        /// <summary>
        /// Purpose: Returns true when the handle has either a name or UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        bool is_valid() const
        {
            return id.is_valid();
        }

        std::string name;
        Uuid id = {};
    };
}
