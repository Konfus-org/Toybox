#pragma once
#include "tbx/tsl/int.h"
#include <initializer_list>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>

namespace tbx
{
    template <typename T>
    struct List
    {
        using Storage = std::list<T>;
        using iterator = typename Storage::iterator;
        using const_iterator = typename Storage::const_iterator;

        List() = default;
        List(uint initial_count)
            : _storage(initial_count)
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
            return _storage.empty() ? nullptr : std::addressof(_storage.front());
        }

        const T* get_raw() const
        {
            return _storage.empty() ? nullptr : std::addressof(_storage.front());
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

        T& operator[](uint index)
        {
            return element_at(index);
        }

        const T& operator[](uint index) const
        {
            return element_at(index);
        }

      private:
        T& element_at(uint index)
        {
            return const_cast<T&>(std::as_const(*this).element_at(index));
        }

        const T& element_at(uint index) const
        {
            if (index >= get_count())
            {
                throw std::out_of_range("tbx::List index out of range");
            }

            auto it = _storage.begin();
            std::advance(it, index);
            return *it;
        }

        Storage _storage;
    };
}
