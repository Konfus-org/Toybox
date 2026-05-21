#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/types/uuid.h"
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
    // TODO: add a is_valid bool that is a shared ptr. Then we can flag invalid when something is
    // put in an invalid state (like deleting an asset, or closing a window, or anything like that)
    class Handle
    {
      public:
        Handle() = default;

        Handle(std::string handle_name)
            : Handle(std::move(handle_name), hash_string_to_id(handle_name))
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
        bool is_valid() const
        {
            return _id.is_valid();
        }

        const std::string& get_name() const
        {
            return _name;
        }

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

        operator std::string() const
        {
            return _name;
        }

      private:
        static Uuid hash_string_to_id(std::string_view handle_name)
        {
            const auto hasher = std::hash<std::string_view>();
            const auto hashed = static_cast<uint32>(hasher(handle_name));
            return hashed == 0U ? Uuid(1U) : Uuid(hashed);
        }

      private:
        std::string _name = {};
        Uuid _id = {};
    };

    // TODO: make logging take into account to_string implementations
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
