#pragma once

namespace std
{
    inline std::size_t hash<tbx::Uuid>::operator()(const tbx::Uuid& value) const
    {
        return hash<tbx::uint32>()(static_cast<tbx::uint32>(value));
    }
}
