#pragma once
#include "tbx/tsl/int.h"
#include <initializer_list>
#include <utility>
#include <vector>

namespace tbx
{
    template <typename T>
    class List
    {
      public:
        using Storage = std::vector<T>;
        using value_type = T;
        using size_type = typename Storage::size_type;
        using iterator = typename Storage::iterator;
        using const_iterator = typename Storage::const_iterator;

        List() = default;
        explicit List(uint initial_count)
            : _storage(static_cast<size_type>(initial_count))
        {
        }
        List(std::initializer_list<T> init)
            : _storage(init)
        {
        }

        List(const List&) = default;
        List(List&&) noexcept = default;
        ~List() = default;

        List& operator=(const List&) = default;
        List& operator=(List&&) noexcept = default;

        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        bool is_empty() const
        {
            return _storage.empty();
        }

        void clear()
        {
            _storage.clear();
        }

        void reserve(uint capacity)
        {
            _storage.reserve(static_cast<size_type>(capacity));
        }

        uint get_capacity() const
        {
            return static_cast<uint>(_storage.capacity());
        }

        void push_back(const T& value)
        {
            _storage.push_back(value);
        }

        void push_back(T&& value)
        {
            _storage.push_back(std::move(value));
        }

        template <typename... Args>
        T& emplace_back(Args&&... args)
        {
            return _storage.emplace_back(std::forward<Args>(args)...);
        }

        void pop_back()
        {
            if (!_storage.empty())
            {
                _storage.pop_back();
            }
        }

        T* get_raw()
        {
            return _storage.empty() ? nullptr : _storage.data();
        }

        const T* get_raw() const
        {
            return _storage.empty() ? nullptr : _storage.data();
        }

        T& front()
        {
            return _storage.front();
        }

        const T& front() const
        {
            return _storage.front();
        }

        T& back()
        {
            return _storage.back();
        }

        const T& back() const
        {
            return _storage.back();
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

        const_iterator cbegin() const
        {
            return _storage.cbegin();
        }

        const_iterator cend() const
        {
            return _storage.cend();
        }

        T& operator[](uint index)
        {
            return _storage[static_cast<size_type>(index)];
        }

        const T& operator[](uint index) const
        {
            return _storage[static_cast<size_type>(index)];
        }

        T& at(uint index)
        {
            return _storage.at(static_cast<size_type>(index));
        }

        const T& at(uint index) const
        {
            return _storage.at(static_cast<size_type>(index));
        }

        Storage& std_vector()
        {
            return _storage;
        }

        const Storage& std_vector() const
        {
            return _storage;
        }

        void swap(List& other)
        {
            _storage.swap(other._storage);
        }

      private:
        Storage _storage;
    };
}
