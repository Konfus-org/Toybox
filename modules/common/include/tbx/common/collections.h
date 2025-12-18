#pragma once
#include "tbx/common/int.h"
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    template <typename TValue, uint Size>
    class Array;

    template <
        typename TKey,
        typename TValue,
        typename THash = std::hash<TKey>,
        typename TKeyEqual = std::equal_to<TKey>,
        typename TAllocator = std::allocator<std::pair<const TKey, TValue>>>
    class HashMap;

    template <
        typename TValue,
        typename THash = std::hash<TValue>,
        typename TKeyEqual = std::equal_to<TValue>,
        typename TAllocator = std::allocator<TValue>>
    class HashSet;

    template <typename TValue, typename TAllocator = std::allocator<TValue>>
    class List;

    template <typename TRange>
    concept LinquableCollection = requires(TRange& range) {
        { range.get_storage() } -> std::ranges::range;
        { std::as_const(range).get_storage() } -> std::ranges::range;
        { range.get_storage().size() } -> std::convertible_to<uint>;
        { std::as_const(range).get_storage().size() } -> std::convertible_to<uint>;
    };

    template <typename TRange, typename TValue>
        requires LinquableCollection<TRange>
    class Linqable
    {
      public:
        template <typename Projection>
        auto select(Projection&& projection) const
        {
            List<std::decay_t<std::invoke_result_t<Projection&, const TValue&>>> results;
            reserve_if_possible(results, get_range());

            for (const auto& value : get_range())
            {
                results.emplace(std::invoke(projection, value));
            }

            return results;
        }

        template <typename Predicate>
        List<TValue> where(Predicate&& predicate) const
        {
            List<TValue> results;

            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    results.emplace(value);
                }
            }

            return results;
        }

        TValue first() const
        {
            for (const auto& value : get_range())
            {
                return value;
            }

            throw std::out_of_range("Linqable::first: range is empty");
        }

        template <typename Predicate>
        TValue first(Predicate&& predicate) const
        {
            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    return value;
                }
            }

            throw std::out_of_range("Linqable::first: predicate did not match any item");
        }

        template <typename TValue>
        TValue first_or_default(TValue default_value) const
        {
            for (const auto& value : get_range())
            {
                return static_cast<TValue>(value);
            }

            return default_value;
        }

        template <typename Predicate, typename TValue>
        TValue first_or_default(Predicate&& predicate, TValue default_value) const
        {
            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    return static_cast<TValue>(value);
                }
            }

            return default_value;
        }

        template <typename Predicate>
        bool any(Predicate&& predicate) const
        {
            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    return true;
                }
            }

            return false;
        }

        bool any() const
        {
            for (const auto& value : get_range())
            {
                (void)value;
                return true;
            }

            return false;
        }

        template <typename Predicate>
        bool all(Predicate&& predicate) const
        {
            for (const auto& value : get_range())
            {
                if (!std::invoke(predicate, value))
                {
                    return false;
                }
            }

            return true;
        }

        template <typename Predicate>
        uint count(Predicate&& predicate) const
        {
            uint total = 0U;

            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    ++total;
                }
            }

            return total;
        }

        template <typename TValue>
        bool contains(const TValue& value) const
        {
            for (const auto& candidate : get_range())
            {
                if (candidate == value)
                {
                    return true;
                }
            }

            return false;
        }

        auto begin()
        {
            return get_storage().begin();
        }

        auto begin() const
        {
            return get_storage().begin();
        }

        auto end()
        {
            return get_storage().end();
        }

        auto end() const
        {
            return get_storage().end();
        }

        auto rbegin()
        {
            return get_storage().rbegin();
        }

        auto rbegin() const
        {
            return get_storage().rbegin();
        }

        auto rend()
        {
            return get_storage().rend();
        }

        auto rend() const
        {
            return get_storage().rend();
        }

      protected:
        const TRange& get_range() const
        {
            return *static_cast<const TRange*>(this);
        }

        TRange& get_range()
        {
            return *static_cast<TRange*>(this);
        }

      private:
        template <std::ranges::range TCollection, std::ranges::range Range>
        void reserve_if_possible(TCollection& collection, const Range& range) const
        {
            if constexpr (std::ranges::sized_range<Range>)
            {
                if constexpr (requires(TCollection& candidate) {
                                  candidate.reserve(static_cast<uint>(0));
                              })
                {
                    collection.reserve(static_cast<uint>(std::ranges::size(range)));
                }
            }
        }

        auto get_storage()
        {
            return get_range().get_storage();
        }

        auto get_storage() const
        {
            return get_range().get_storage();
        }
    };

    template <typename TValue, uint Size>
    class Array : public Linqable<Array<TValue, Size>, TValue>
    {
      public:
        Array() = default;
        Array(std::initializer_list<TValue> values)
        {
            for (const auto& value : values)
            {
                add(value);
            }
        }

        void add(const TValue& value)
        {
            for (uint index = 0; index < _storage.size(); index++)
            {
                const auto* item = &storage[index];
                if (item == nullptr)
                    storage[index] = value;
            }

            throw std::out_of_range("Array::add: array is full!");
        }

        void remove(uint position)
        {
            _storage.erase(position);
        }

        bool remove(const TValue& value)
        {
            return _storage.erase(value) > 0U;
        }

        void clear()
        {
            _storage.fill(TValue());
        }

        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        bool is_empty() const
        {
            return _storage.empty();
        }

        TValue& operator[](uint index)
        {
            return _storage.at(index);
        }

        const TValue& operator[](uint index) const
        {
            return _storage.at(index);
        }

      private:
        friend class Linqable<Array<TValue, Size>, TValue>;
        std::array<TValue, Size> _storage = {};

      private:
        std::span<TValue> get_storage()
        {
            return std::span<TValue>(_storage);
        }

        std::span<const TValue> get_storage() const
        {
            return std::span<const TValue>(_storage);
        }
    };

    template <
        typename TKey,
        typename TValue,
        typename THash,
        typename TKeyEqual,
        typename TAllocator>
    class HashMap
        : public Linqable<
              HashMap<TKey, TValue, THash, TKeyEqual, TAllocator>,
              std::pair<const TKey, TValue>>
    {
      public:
        HashMap() = default;
        HashMap(std::initializer_list<std::pair<const TKey, TValue>> values)
        {
            for (const auto& entry : values)
            {
                add(entry.first, entry.second);
            }
        }

        bool add(const TKey& key, const TValue& value)
        {
            return _storage.emplace(key, value).second;
        }

        template <typename... TArgs>
        bool emplace(const TKey& key, TArgs&&... args)
        {
            return _storage
                .emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(key),
                    std::forward_as_tuple(std::forward<TArgs>(args)...))
                .second;
        }

        bool remove(const TKey& key)
        {
            return _storage.erase(key) > 0U;
        }

        void clear()
        {
            _storage.clear();
        }

        void reserve(uint count)
        {
            _storage.reserve(count);
        }

        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        bool is_empty() const
        {
            return _storage.empty();
        }

        TValue& operator[](const TKey& key)
        {
            return _storage[key];
        }

        const TValue& operator[](const TKey& key) const
        {
            return _storage.at(key);
        }

      private:
        friend class Linqable<
            HashMap<TKey, TValue, THash, TKeyEqual, TAllocator>,
            std::pair<const TKey, TValue>>;
        std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator> _storage;

      private:
        std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>& get_storage()
        {
            return _storage;
        }

        const std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>& get_storage() const
        {
            return _storage;
        }
    };

    template <typename TValue, typename THash, typename TKeyEqual, typename TAllocator>
    class HashSet : public Linqable<HashSet<TValue, THash, TKeyEqual, TAllocator>, TValue>
    {
      public:
        HashSet() = default;
        HashSet(std::initializer_list<TValue> values)
        {
            for (const auto& value : values)
            {
                add(value);
            }
        }

        bool add(const TValue& value)
        {
            return _storage.insert(value).second;
        }

        template <typename... TArgs>
        bool emplace(TArgs&&... args)
        {
            return _storage.emplace(std::forward<TArgs>(args)...).second;
        }

        bool remove(const TValue& value)
        {
            return _storage.erase(value) > 0U;
        }

        void remove(uint position)
        {
            _storage.erase(position);
        }

        void clear()
        {
            _storage.clear();
        }

        void reserve(uint count)
        {
            _storage.reserve(count);
        }

        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        bool is_empty() const
        {
            return _storage.empty();
        }

        TValue& operator[](uint i)
        {
            return _storage[i];
        }

        const TValue& operator[](uint i) const
        {
            return _storage.at(i);
        }

      private:
        friend class Linqable<HashSet<TValue, THash, TKeyEqual, TAllocator>, TValue>;
        std::unordered_set<TValue, THash, TKeyEqual, TAllocator> _storage;

      private:
        std::unordered_set<TValue, THash, TKeyEqual, TAllocator>& get_storage()
        {
            return _storage;
        }

        const std::unordered_set<TValue, THash, TKeyEqual, TAllocator>& get_storage() const
        {
            return _storage;
        }
    };

    template <typename TValue, typename TAllocator>
    class List : public Linqable<List<TValue, TAllocator>, TValue>
    {
      public:
        List() = default;
        List(std::initializer_list<TValue> values)
            : _storage(values)
        {
        }
        List(uint count)
            : _storage(count)
        {
        }
        List(uint count, const TValue& value)
            : _storage(count, value)
        {
        }

        bool add(const TValue& value)
        {
            _storage.push_back(value);
            return true;
        }

        bool add(TValue&& value)
        {
            _storage.push_back(std::move(value));
            return true;
        }

        template <typename... TArgs>
        bool emplace(TArgs&&... args)
        {
            _storage.emplace_back(std::forward<TArgs>(args)...);
            return true;
        }

        void remove(const TValue& value) {}

        void remove(uint position)
        {
            _storage.erase(position);
        }

        void clear()
        {
            _storage.clear();
        }

        void reserve(uint count)
        {
            _storage.reserve(count);
        }

        bool is_empty() const
        {
            return _storage.empty();
        }

        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        TValue& operator[](uint index)
        {
            return _storage.at(index);
        }

        const TValue& operator[](uint index) const
        {
            return _storage.at(index);
        }

      private:
        friend class Linqable<List<TValue, TAllocator>, TValue>;
        std::vector<TValue, TAllocator> _storage;

      private:
        std::vector<TValue, TAllocator>& get_storage()
        {
            return _storage;
        }

        const std::vector<TValue, TAllocator>& get_storage() const
        {
            return _storage;
        }
    };
}
