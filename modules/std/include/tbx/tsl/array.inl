#pragma once

namespace tbx
{
    template <typename T, std::size_t Size>
    inline Array<T, Size>::Array() = default;

    template <typename T, std::size_t Size>
    inline Array<T, Size>::Array(std::initializer_list<T> init)
    {
        auto src = init.begin();
        size_t index = 0;
        while (src != init.end() && index < Size)
        {
            _storage[index++] = *src++;
        }
    }

    template <typename T, std::size_t Size>
    inline constexpr uint Array<T, Size>::get_count()
    {
        return static_cast<uint>(Size);
    }

    template <typename T, std::size_t Size>
    inline T* Array<T, Size>::get_raw()
    {
        return _storage.data();
    }

    template <typename T, std::size_t Size>
    inline const T* Array<T, Size>::get_raw() const
    {
        return _storage.data();
    }

    template <typename T, std::size_t Size>
    inline typename Array<T, Size>::iterator Array<T, Size>::begin()
    {
        return _storage.begin();
    }

    template <typename T, std::size_t Size>
    inline typename Array<T, Size>::iterator Array<T, Size>::end()
    {
        return _storage.end();
    }

    template <typename T, std::size_t Size>
    inline typename Array<T, Size>::const_iterator Array<T, Size>::begin() const
    {
        return _storage.begin();
    }

    template <typename T, std::size_t Size>
    inline typename Array<T, Size>::const_iterator Array<T, Size>::end() const
    {
        return _storage.end();
    }

    template <typename T, std::size_t Size>
    inline T& Array<T, Size>::operator[](uint index)
    {
        return _storage[index];
    }

    template <typename T, std::size_t Size>
    inline const T& Array<T, Size>::operator[](uint index) const
    {
        return _storage[index];
    }
}
