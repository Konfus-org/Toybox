#pragma once

inline ::size std::hash<tbx::Uuid>::operator()(const tbx::Uuid& value) const
{
    return std::hash<::uint32>()(static_cast<::uint32>(value));
}
