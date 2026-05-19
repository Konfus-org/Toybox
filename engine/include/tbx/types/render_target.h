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
    };
}

namespace std
{
    template <>
    struct hash<tbx::RenderTarget>
    {
        ::size operator()(const tbx::RenderTarget& value) const
        {
            return hash<tbx::Handle>()(value);
        }
    };
}
