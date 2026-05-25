#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/types/uuid.h"
#include <atomic>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a general handle that references assets or runtime objects by stable
    /// name and id.
    /// @details
    /// Ownership: Stores owned name strings, UUID values, and shared validity state.
    /// Thread Safety: Safe to copy between threads; invalidation is atomic.
    struct Handle
    {
        Handle() = default;

        Handle(std::string handle_name)
            : name(std::move(handle_name))
            , id(hash_string_to_id(name))
        {
        }

        Handle(Uuid handle_id)
            : id(handle_id)
        {
        }

        Handle(std::string handle_name, Uuid handle_id)
            : name(std::move(handle_name))
            , id(handle_id)
        {
        }

        bool operator==(const Handle& other) const
        {
            return id == other.id && name == other.name;
        }

        bool is_valid() const
        {
            return id.is_valid() && _is_valid && _is_valid->load();
        }

        void invalidate() const
        {
            if (_is_valid)
                _is_valid->store(false);
        }

        std::string name = {};
        Uuid id = {};

      private:
        std::shared_ptr<std::atomic_bool> _is_valid = std::make_shared<std::atomic_bool>(true);
    };

    TBX_SERIALIZABLE_STRUCT(Handle, name, id)
}

template <>
struct std::formatter<tbx::Handle>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::Handle& value, TFormatContext& ctx) const
    {
        if (!value.name.empty())
            return _formatter.format(value.name, ctx);

        return _formatter.format(std::format("{}", value.id), ctx);
    }

    std::formatter<std::string> _formatter;
};

template <>
struct std::hash<tbx::Handle>
{
    ::size operator()(const tbx::Handle& value) const
    {
        auto seed = hash<tbx::Uuid>()(value.id);
        seed ^= hash<std::string>()(value.name) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        return seed;
    }
};
