#pragma once
#include "tbx/std/int.h"
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

        List();
        explicit List(uint initial_count);
        List(std::initializer_list<T> init);

        List(const List&) = default;
        List(List&&) noexcept = default;
        ~List() = default;

        List& operator=(const List&) = default;
        List& operator=(List&&) noexcept = default;

        uint get_count() const;

        bool is_empty() const;

        void clear();

        void reserve(uint capacity);

        uint get_capacity() const;

        void push_back(const T& value);

        void push_back(T&& value);

        template <typename... Args>
        T& emplace_back(Args&&... args);

        void pop_back();

        T* get_raw();

        const T* get_raw() const;

        T& front();

        const T& front() const;

        T& back();

        const T& back() const;

        iterator begin();

        iterator end();

        const_iterator begin() const;

        const_iterator end() const;

        const_iterator cbegin() const;

        const_iterator cend() const;

        T& operator[](uint index);

        const T& operator[](uint index) const;

        T& at(uint index);

        const T& at(uint index) const;

        Storage& std_vector();

        const Storage& std_vector() const;

        void swap(List& other);

      private:
        Storage _storage;
    };
}

#include "../../../src/std/list.inl"
