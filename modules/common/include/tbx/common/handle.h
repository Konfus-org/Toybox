#pragma once
#include "tbx/common/uuid.h"
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a general handle that references assets or runtime objects by stable
    /// name and id.
    /// @details
    /// Ownership: Stores owned name strings and UUID values.
    /// Thread Safety: Safe to copy between threads; there are no mutating public APIs.
    class Handle
    {
      public:
        Handle() = default;

        Handle(std::string handle_name)
            : Handle(std::move(handle_name), hash_name_to_id(handle_name))
        {
        }

        Handle(Uuid handle_id)
            : _id(handle_id)
        {
        }

        Handle(std::string handle_name, Uuid handle_id)
            : _name(std::move(handle_name))
            , _id(handle_id)
        {
        }

      public:
        /// @brief
        /// Purpose: Returns true when the handle has a valid id.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Safe to call concurrently.
        bool is_valid() const
        {
            return _id.is_valid();
        }

        /// @brief
        /// Purpose: Returns the immutable logical name associated with the handle.
        /// @details
        /// Ownership: Returns a const reference owned by the handle.
        /// Thread Safety: Safe to call concurrently.
        const std::string& get_name() const
        {
            return _name;
        }

        /// @brief
        /// Purpose: Returns the immutable id associated with the handle.
        /// @details
        /// Ownership: Returns a const reference owned by the handle.
        /// Thread Safety: Safe to call concurrently.
        const Uuid& get_id() const
        {
            return _id;
        }

        bool operator==(const Handle& other) const
        {
            return _id == other._id && _name == other._name;
        }

        bool operator!=(const Handle& other) const
        {
            return !(*this == other);
        }

      public:
        static Uuid hash_name_to_id(std::string_view handle_name)
        {
            const auto hasher = std::hash<std::string_view>();
            const auto hashed = static_cast<uint32>(hasher(handle_name));
            return hashed == 0U ? Uuid(1U) : Uuid(hashed);
        }

      private:
        std::string _name = {};
        Uuid _id = {};
    };

    inline std::string to_string(const Handle& value)
    {
        if (!value.get_name().empty())
            return value.get_name();

        return to_string(value.get_id());
    }
}

namespace std
{
    template <>
    struct hash<tbx::Handle>
    {
        ::size operator()(const tbx::Handle& value) const
        {
            auto seed = hash<tbx::Uuid>()(value.get_id());
            seed ^= hash<std::string>()(value.get_name()) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}
