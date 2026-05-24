#pragma once

namespace tbx
{
    template <typename T>
    T SharedLibrary::get_symbol(const char* name) const
    {
        static_assert(std::is_pointer_v<T>, "get_symbol<T> requires a pointer type");
        return reinterpret_cast<T>(get_symbol_raw(name));
    }
}
