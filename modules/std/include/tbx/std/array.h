#pragma once
#include "tbx/std/int.h"
#include <array>
#include <cstddef>
#include <initializer_list>
#include <utility>

namespace tbx
{
    template <typename T, std::size_t Size>
    struct Array
    {
        using Storage = std::array<T, Size>;
        using iterator = typename Storage::iterator;
        using const_iterator = typename Storage::const_iterator;

        Array() = default;
        Array(const Array&) = default;
        Array(Array&&) noexcept = default;
        ~Array() = default;
        Array(std::initializer_list<T> init)
        {
            auto src = init.begin();
            size_t index = 0;
            while (src != init.end() && index < Size)
            {
                _storage[index++] = *src++;
            }
        }

        static constexpr uint get_count()
        {
            return static_cast<uint>(Size);
        }

        T* get_raw()
        {
            return _storage.data();
        }

        const T* get_raw() const
        {
            return _storage.data();
        }

        iterator begin()
        {
            return _storage.begin();
        }

        iterator end()
        {
            return _storage.end();
        }

        const_iterator begin() const
        {
            return _storage.begin();
        }

        const_iterator end() const
        {
            return _storage.end();
        }

        Array& operator=(const Array&) = default;
        Array& operator=(Array&&) noexcept = default;

        T& operator[](uint index)
        {
            return _storage[index];
        }

        const T& operator[](uint index) const
        {
            return _storage[index];
        }

      private:
        Storage _storage = {};
    };
}
