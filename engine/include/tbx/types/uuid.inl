#pragma once

namespace std
{
    inline ::size hash<tbx::Uuid>::operator()(const tbx::Uuid& value) const
    {
        return hash<::uint32>()(static_cast<::uint32>(value));
    }
}
