#pragma once
#include "tbx/tsl/int.h"
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

        Array();
        Array(const Array&) = default;
        Array(Array&&) noexcept = default;
        ~Array() = default;
        Array(std::initializer_list<T> init);

        static constexpr uint get_count();

        T* get_raw();

        const T* get_raw() const;

        iterator begin();

        iterator end();

        const_iterator begin() const;

        const_iterator end() const;

        Array& operator=(const Array&) = default;
        Array& operator=(Array&&) noexcept = default;

        T& operator[](uint index);

        const T& operator[](uint index) const;

      private:
        Storage _storage = {};
    };
}

#include "tbx/tsl/array.inl"
