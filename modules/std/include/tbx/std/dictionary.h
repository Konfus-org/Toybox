#pragma once
#include "tbx/std/int.h"
#include <initializer_list>
#include <unordered_map>
#include <utility>

namespace tbx
{
    template <typename TKey,
              typename TValue,
              typename THash = std::hash<TKey>,
              typename TKeyEqual = std::equal_to<TKey>,
              typename TAllocator = std::allocator<std::pair<const TKey, TValue>>>
    class Dictionary
    {
      public:
        using Storage = std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>;
        using value_type = typename Storage::value_type;
        using iterator = typename Storage::iterator;
        using const_iterator = typename Storage::const_iterator;

        Dictionary() = default;
        Dictionary(std::initializer_list<value_type> init)
            : _storage(init)
        {
        }

        Dictionary(const Dictionary&) = default;
        Dictionary(Dictionary&&) noexcept = default;
        Dictionary& operator=(const Dictionary&) = default;
        Dictionary& operator=(Dictionary&&) noexcept = default;

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

        bool contains(const TKey& key) const
        {
            return _storage.find(key) != _storage.end();
        }

        TValue& operator[](const TKey& key)
        {
            return _storage[key];
        }

        TValue& get(const TKey& key)
        {
            return _storage.at(key);
        }

        const TValue& get(const TKey& key) const
        {
            return _storage.at(key);
        }

        iterator at(const TKey& key)
        {
            return _storage.find(key);
        }

        const_iterator at(const TKey& key) const
        {
            return _storage.find(key);
        }

        bool contains(const TKey& key)
        {
            return _storage.contains(key);
        }

        void insert(const value_type& value)
        {
            _storage.insert(value);
        }

        void insert(value_type&& value)
        {
            _storage.insert(std::move(value));
        }

        template <typename... TArgs>
        void emplace(TArgs&&... args)
        {
            _storage.emplace(std::forward<TArgs>(args)...);
        }

        void erase(const TKey& key)
        {
            _storage.erase(key);
        }

        void erase(iterator it)
        {
            _storage.erase(it);
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

        Storage& std_unordered_map()
        {
            return _storage;
        }

        const Storage& std_unordered_map() const
        {
            return _storage;
        }

      private:
        Storage _storage;
    };
}
