#pragma once
#include "tbx/types/handle.h"
#include <functional>

namespace tbx
{
    struct RenderTarget : Handle
    {
        RenderTarget() = default;
        using Handle::Handle;
        RenderTarget(const Handle& handle)
            : Handle(handle)
        {
        }
        RenderTarget(Handle&& handle)
            : Handle(std::move(handle))
        {
        }

        bool operator==(const RenderTarget& other) const
        {
            return static_cast<const Handle&>(*this) == static_cast<const Handle&>(other);
        }
    };
}

template <>
struct std::hash<tbx::RenderTarget>
{
    ::size operator()(const tbx::RenderTarget& value) const
    {
        return hash<tbx::Handle>()(value);
    }
};

template <>
struct std::formatter<tbx::RenderTarget>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::RenderTarget& value, TFormatContext& ctx) const
    {
        return _formatter.format(static_cast<const tbx::Handle&>(value), ctx);
    }

    std::formatter<tbx::Handle> _formatter;
};
