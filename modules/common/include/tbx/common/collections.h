#pragma once
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    template <typename TValue, std::size_t Size>
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
    using range_value = std::ranges::range_value_t<TRange>;

    template <typename TRange>
    class Linqable
    {
    public:
        /// Provides LINQ-style helper methods for range-like containers.
        /// Purpose: exposes projection, filtering, and lookup helpers mirroring the C# LINQ surface.
        /// Ownership: returned collections own their elements; references passed to predicates remain non-owning.
        /// Thread Safety: not thread-safe; callers must synchronize external access and mutation.
        template <typename Projection>
        auto select(Projection&& projection) const
        {
            using range_type = decltype(get_range());
            using value_type = range_value<range_type>;
            using result_value =
                std::decay_t<std::invoke_result_t<Projection&, const value_type&>>;

            List<result_value> results;
            reserve_if_possible(results, get_range());

            for (const auto& value : get_range())
            {
                results.emplace(std::invoke(projection, value));
            }

            return results;
        }

        /// Filters the current range using the provided predicate.
        /// Purpose: returns a new collection containing only elements that satisfy the predicate.
        /// Ownership: the resulting collection owns its stored values.
        /// Thread Safety: not thread-safe; synchronize access when iterating concurrently.
        template <typename Predicate>
        auto where(Predicate&& predicate) const
        {
            using range_type = decltype(get_range());
            using value_type = range_value<range_type>;

            List<value_type> results;

            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    results.emplace(value);
                }
            }

            return results;
        }

        /// Projects the first element from the current range.
        /// Purpose: retrieves the first item, throwing if the range is empty.
        /// Ownership: returns a copy of the stored value.
        /// Thread Safety: not thread-safe; external synchronization is required for concurrent access.
        auto first() const
        {
            for (const auto& value : get_range())
            {
                return value;
            }

            throw std::out_of_range("linq::first: range is empty");
        }

        /// Projects the first matching element from the current range.
        /// Purpose: retrieves the first item satisfying the predicate or throws if no match exists.
        /// Ownership: returns a copy of the stored value.
        /// Thread Safety: not thread-safe; external synchronization is required for concurrent access.
        template <typename Predicate>
        auto first(Predicate&& predicate) const
        {
            for (const auto& value : get_range())
            {
                if (std::invoke(predicate, value))
                {
                    return value;
                }
            }

            throw std::out_of_range("linq::first: predicate did not match any item");
        }

        /// Projects the first element or a default value when empty.
        /// Purpose: avoids throwing by returning the provided default when no items are present.
        /// Ownership: returns a copy of either the stored value or the provided default.
        /// Thread Safety: not thread-safe; external synchronization is required for concurrent access.
        template <typename TValue>
        TValue first_or_default(TValue default_value) const
        {
            for (const auto& value : get_range())
            {
                return static_cast<TValue>(value);
            }

            return default_value;
        }

        /// Projects the first matching element or a default value when no match is found.
        /// Purpose: avoids throwing by returning the provided default when the predicate does not match.
        /// Ownership: returns a copy of either the stored value or the provided default.
        /// Thread Safety: not thread-safe; external synchronization is required for concurrent access.
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

        /// Determines whether any element in the range satisfies the predicate.
        /// Purpose: evaluates the predicate until a match is found.
        /// Ownership: does not take ownership of referenced values.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
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

        /// Determines whether the range contains any elements.
        /// Purpose: reports whether at least one item exists in the container.
        /// Ownership: does not take ownership of referenced values.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
        bool any() const
        {
            for (const auto& value : get_range())
            {
                (void)value;
                return true;
            }

            return false;
        }

        /// Determines whether all elements satisfy the predicate.
        /// Purpose: evaluates the predicate across the entire range.
        /// Ownership: does not take ownership of referenced values.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
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

        /// Counts the number of elements.
        /// Purpose: reports the total size of the range.
        /// Ownership: does not take ownership of referenced values.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
        uint count() const
        {
            uint total = 0U;

            for (const auto& value : get_range())
            {
                (void)value;
                ++total;
            }

            return total;
        }

        /// Counts the number of elements satisfying the predicate.
        /// Purpose: reports how many items meet the predicate criteria.
        /// Ownership: does not take ownership of referenced values.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
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

        /// Determines whether the range contains the provided value.
        /// Purpose: checks for equality against each item.
        /// Ownership: does not take ownership of referenced values.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
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

        /// Converts the range into a list.
        /// Purpose: materializes the range into a contiguous List wrapper.
        /// Ownership: the returned list owns its contents.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads and writes.
        auto to_list() const
        {
            using range_type = decltype(get_range());
            using value_type = range_value<range_type>;

            List<value_type> results;
            reserve_if_possible(results, get_range());

            for (const auto& value : get_range())
            {
                results.emplace(value);
            }

            return results;
        }

        /// Converts the range into a fixed-capacity array.
        /// Purpose: materializes the range into an Array wrapper up to the provided capacity.
        /// Ownership: the returned array owns copied entries.
        /// Thread Safety: not thread-safe; synchronize access for concurrent reads or writes.
        template <std::size_t Size>
        auto to_array() const
        {
            using range_type = decltype(get_range());
            using value_type = range_value<range_type>;

            Array<value_type, Size> results;

            for (const auto& value : get_range())
            {
                if (!results.emplace(value))
                {
                    break;
                }
            }

            return results;
        }

        /// Converts the range into a hash-backed map.
        /// Purpose: materializes the range into a HashMap by treating each entry as a key/value
        /// pair.
        /// Ownership: the returned map owns copied keys and values.
        /// Thread Safety: not thread-safe; synchronize external access for concurrent reads or
        /// writes.
        auto to_hash_map() const
        {
            using range_type = decltype(get_range());
            using entry_type = range_value<range_type>;
            using key_type = std::decay_t<decltype(std::declval<entry_type>().first)>;
            using value_type = std::decay_t<decltype(std::declval<entry_type>().second)>;

            using allocator_type = std::allocator<std::pair<const key_type, value_type>>;
            using map_type = HashMap<
                key_type,
                value_type,
                std::hash<key_type>,
                std::equal_to<key_type>,
                allocator_type>;

            map_type results;

            for (const auto& entry : get_range())
            {
                results.emplace(entry.first, entry.second);
            }

            return results;
        }

        /// Converts the range into a hash-backed set.
        /// Purpose: materializes the range into a HashSet, preserving unique values.
        /// Ownership: the returned set owns copied entries.
        /// Thread Safety: not thread-safe; synchronize external access for concurrent reads or
        /// writes.
        auto to_hash_set() const
        {
            using range_type = decltype(get_range());
            using value_type = range_value<range_type>;

            using set_type = HashSet<
                value_type,
                std::hash<value_type>,
                std::equal_to<value_type>,
                std::allocator<value_type>>;

            set_type results;

            for (const auto& value : get_range())
            {
                results.emplace(value);
            }

            return results;
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

        decltype(auto) get_storage()
        {
            return get_range().get_storage();
        }

        decltype(auto) get_storage() const
        {
            return get_range().get_storage();
        }
    };

    template <typename TValue, std::size_t Size>
    class Array : public Linqable<Array<TValue, Size>>
    {
    public:
        using iterator = typename std::array<TValue, Size>::iterator;
        using const_iterator = typename std::array<TValue, Size>::const_iterator;

        /// Wraps std::array with LINQ-style helpers.
        /// Purpose: exposes fixed-size storage with the linqable surface while offering a consistent
        /// C#-like mutation API.
        /// Ownership: owns contained values in-place.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        Array() = default;

        /// Initializes the array from an initializer list.
        /// Purpose: enables aggregate-style creation while tracking the active item count.
        /// Ownership: copies the provided values into owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        Array(std::initializer_list<TValue> values)
        {
            add_range(values.begin(), values.end());
        }

        /// Adds a value to the next available slot when capacity permits.
        /// Purpose: provides a consistent append-style API resembling C# collection Add.
        /// Ownership: copies the provided value into owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        bool add(const TValue& value)
        {
            return emplace(value);
        }

        /// Emplaces a value to the next available slot when capacity permits.
        /// Purpose: constructs the stored value in place to avoid redundant copies.
        /// Ownership: owns the in-place constructed value.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        template <typename... TArgs>
        bool emplace(TArgs&&... args)
        {
            if (_size >= Size)
            {
                return false;
            }

            _storage[_size] = TValue(std::forward<TArgs>(args)...);
            ++_size;
            return true;
        }

        /// Clears the tracked items without altering capacity.
        /// Purpose: resets the logical contents while reusing allocated storage.
        /// Ownership: retains ownership of storage; values beyond the tracked size remain defaulted.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        void clear()
        {
            _size = 0U;
        }

        /// Retrieves the number of active items.
        /// Purpose: reports how many elements participate in iteration and LINQ queries.
        /// Ownership: does not transfer ownership.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        uint get_count() const
        {
            return _size;
        }

        /// Retrieves a reference to the stored value at the provided index.
        /// Purpose: matches the C# indexer semantics while using explicit Get prefix.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        TValue& get_at(uint index)
        {
            return _storage.at(index);
        }

        /// Retrieves a const reference to the stored value at the provided index.
        /// Purpose: matches the C# indexer semantics while using explicit Get prefix.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        const TValue& get_at(uint index) const
        {
            return _storage.at(index);
        }

        /// Provides array-style access to stored values.
        /// Purpose: aligns with square bracket usage expectations similar to C# indexers.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        TValue& operator[](uint index)
        {
            return get_at(index);
        }

        /// Provides read-only array-style access to stored values.
        /// Purpose: aligns with square bracket usage expectations similar to C# indexers.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        const TValue& operator[](uint index) const
        {
            return get_at(index);
        }

        /// Combines two arrays into a single fixed-capacity array.
        /// Purpose: supports C#-style additive composition by appending values until capacity is
        /// reached.
        /// Ownership: the returned array owns copied values; inputs retain ownership of their
        /// storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        Array<TValue, Size> operator+(const Array<TValue, Size>& other) const
        {
            Array<TValue, Size> combined;
            combined.add_range(this->begin(), this->end());
            combined.add_range(other.begin(), other.end());
            return combined;
        }

    private:
        friend class Linqable<Array<TValue, Size>>;

        class RangeView
        {
        public:
            RangeView(std::array<TValue, Size>& storage, uint size)
                : _storage(storage)
                , _size(size)
            {
            }

            iterator begin()
            {
                return _storage.begin();
            }

            const_iterator begin() const
            {
                return _storage.begin();
            }

            iterator end()
            {
                return _storage.begin() + static_cast<ptrdiff_t>(_size);
            }

            const_iterator end() const
            {
                return _storage.begin() + static_cast<ptrdiff_t>(_size);
            }

            auto rbegin()
            {
                return std::make_reverse_iterator(end());
            }

            auto rbegin() const
            {
                return std::make_reverse_iterator(end());
            }

            auto rend()
            {
                return std::make_reverse_iterator(begin());
            }

            auto rend() const
            {
                return std::make_reverse_iterator(begin());
            }

        private:
            std::array<TValue, Size>& _storage;
            uint _size = 0U;
        };

        class ConstRangeView
        {
        public:
            ConstRangeView(const std::array<TValue, Size>& storage, uint size)
                : _storage(storage)
                , _size(size)
            {
            }

            const_iterator begin() const
            {
                return _storage.begin();
            }

            const_iterator end() const
            {
                return _storage.begin() + static_cast<ptrdiff_t>(_size);
            }

            auto rbegin() const
            {
                return std::make_reverse_iterator(end());
            }

            auto rend() const
            {
                return std::make_reverse_iterator(begin());
            }

        private:
            const std::array<TValue, Size>& _storage;
            uint _size = 0U;
        };

        template <typename TIterator>
        void add_range(TIterator begin_iterator, TIterator end_iterator)
        {
            for (auto iterator = begin_iterator; iterator != end_iterator; ++iterator)
            {
                if (!emplace(*iterator))
                {
                    break;
                }
            }
        }

        std::array<TValue, Size> _storage = {};
        uint _size = 0U;

        auto get_storage()
        {
            return RangeView(_storage, _size);
        }

        auto get_storage() const
        {
            return ConstRangeView(_storage, _size);
        }
    };

    template <typename TKey, typename TValue, typename THash, typename TKeyEqual, typename TAllocator>
    class HashMap : public Linqable<HashMap<TKey, TValue, THash, TKeyEqual, TAllocator>>
    {
    public:
        using iterator = typename std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>::iterator;
        using const_iterator = typename std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator>::const_iterator;

        /// Wraps std::unordered_map with LINQ-style helpers.
        /// Purpose: exposes hash-based key/value storage with the linqable surface while presenting
        /// a consistent C#-like API.
        /// Ownership: owns stored key/value pairs.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        HashMap() = default;

        /// Initializes the map from an initializer list.
        /// Purpose: supports concise map creation using Add-style semantics.
        /// Ownership: copies the provided entries into owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        HashMap(std::initializer_list<std::pair<const TKey, TValue>> values)
        {
            add_range(values.begin(), values.end());
        }

        /// Adds a key/value pair when the key does not already exist.
        /// Purpose: mirrors C# Dictionary Add, returning whether insertion succeeded.
        /// Ownership: copies the provided key and value into owned storage on success.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        bool add(const TKey& key, const TValue& value)
        {
            return _storage.emplace(key, value).second;
        }

        /// Emplaces a key/value pair when the key does not already exist.
        /// Purpose: constructs the mapped value in-place using forwarded arguments.
        /// Ownership: owns the emplaced value on success.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
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

        /// Removes a key when present.
        /// Purpose: provides a predictable remove operation mirroring C# collection semantics.
        /// Ownership: releases the removed entry.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        bool remove(const TKey& key)
        {
            return _storage.erase(key) > 0U;
        }

        /// Clears all entries.
        /// Purpose: resets the container to an empty state.
        /// Ownership: retains ownership of storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        void clear()
        {
            _storage.clear();
        }

        /// Retrieves the number of stored entries.
        /// Purpose: reports the active item count for sizing and LINQ operations.
        /// Ownership: does not transfer ownership.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        /// Retrieves a mutable reference to the value for the provided key, inserting a default if
        /// necessary.
        /// Purpose: matches the C# indexer experience while keeping explicit Get naming.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        TValue& get_or_add(const TKey& key)
        {
            return _storage[key];
        }

        /// Retrieves a const reference to the value for the provided key, throwing when missing.
        /// Purpose: provides predictable access semantics aligned with C# Dictionary.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        const TValue& get_at(const TKey& key) const
        {
            return _storage.at(key);
        }

        /// Provides map-style access to stored values, adding a default when missing.
        /// Purpose: mirrors the C# indexer pattern using square bracket syntax.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        TValue& operator[](const TKey& key)
        {
            return get_or_add(key);
        }

        /// Provides read-only map-style access to stored values.
        /// Purpose: mirrors the C# indexer pattern using square bracket syntax.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        const TValue& operator[](const TKey& key) const
        {
            return get_at(key);
        }

        /// Combines two maps into a single dictionary-style collection.
        /// Purpose: supports C#-style additive composition by appending entries while preserving
        /// existing keys.
        /// Ownership: the returned map owns copied entries; inputs retain ownership of their
        /// storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        HashMap<TKey, TValue, THash, TKeyEqual, TAllocator> operator+(
            const HashMap<TKey, TValue, THash, TKeyEqual, TAllocator>& other) const
        {
            HashMap<TKey, TValue, THash, TKeyEqual, TAllocator> combined;
            combined.add_range(this->begin(), this->end());
            combined.add_range(other.begin(), other.end());
            return combined;
        }

    private:
        friend class Linqable<HashMap<TKey, TValue, THash, TKeyEqual, TAllocator>>;

        template <typename TIterator>
        void add_range(TIterator begin_iterator, TIterator end_iterator)
        {
            for (auto iterator = begin_iterator; iterator != end_iterator; ++iterator)
            {
                add(iterator->first, iterator->second);
            }
        }

        std::unordered_map<TKey, TValue, THash, TKeyEqual, TAllocator> _storage;

        auto& get_storage()
        {
            return _storage;
        }

        const auto& get_storage() const
        {
            return _storage;
        }
    };

    template <typename TValue, typename THash, typename TKeyEqual, typename TAllocator>
    class HashSet : public Linqable<HashSet<TValue, THash, TKeyEqual, TAllocator>>
    {
    public:
        using iterator = typename std::unordered_set<TValue, THash, TKeyEqual, TAllocator>::iterator;
        using const_iterator = typename std::unordered_set<TValue, THash, TKeyEqual, TAllocator>::const_iterator;

        /// Wraps std::unordered_set with LINQ-style helpers.
        /// Purpose: exposes hash-based unique storage with the linqable surface while providing a
        /// consistent C#-like API.
        /// Ownership: owns stored values.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        HashSet() = default;

        /// Initializes the set from an initializer list.
        /// Purpose: allows aggregate-style creation while respecting uniqueness.
        /// Ownership: copies the provided values into owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        HashSet(std::initializer_list<TValue> values)
        {
            add_range(values.begin(), values.end());
        }

        /// Adds a value if it is not already present.
        /// Purpose: mirrors C# HashSet Add semantics by returning insertion success.
        /// Ownership: copies the provided value into owned storage on success.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        bool add(const TValue& value)
        {
            return _storage.insert(value).second;
        }

        /// Emplaces a value if it is not already present.
        /// Purpose: constructs the stored value in place to avoid redundant copies.
        /// Ownership: owns the in-place constructed value on success.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        template <typename... TArgs>
        bool emplace(TArgs&&... args)
        {
            return _storage.emplace(std::forward<TArgs>(args)...).second;
        }

        /// Removes a value when present.
        /// Purpose: supplies predictable removal semantics consistent with C# collections.
        /// Ownership: releases the removed element.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        bool remove(const TValue& value)
        {
            return _storage.erase(value) > 0U;
        }

        /// Clears all elements.
        /// Purpose: resets the container to an empty state.
        /// Ownership: retains ownership of storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        void clear()
        {
            _storage.clear();
        }

        /// Retrieves the number of stored elements.
        /// Purpose: reports the active item count for sizing and LINQ operations.
        /// Ownership: does not transfer ownership.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        /// Combines two sets into a single collection containing the union of values.
        /// Purpose: supports C#-style additive composition by appending unique entries.
        /// Ownership: the returned set owns copied values; inputs retain ownership of their
        /// storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        HashSet<TValue, THash, TKeyEqual, TAllocator> operator+(
            const HashSet<TValue, THash, TKeyEqual, TAllocator>& other) const
        {
            HashSet<TValue, THash, TKeyEqual, TAllocator> combined;
            combined.add_range(this->begin(), this->end());
            combined.add_range(other.begin(), other.end());
            return combined;
        }

    private:
        friend class Linqable<HashSet<TValue, THash, TKeyEqual, TAllocator>>;

        template <typename TIterator>
        void add_range(TIterator begin_iterator, TIterator end_iterator)
        {
            for (auto iterator = begin_iterator; iterator != end_iterator; ++iterator)
            {
                add(*iterator);
            }
        }

        std::unordered_set<TValue, THash, TKeyEqual, TAllocator> _storage;

        auto& get_storage()
        {
            return _storage;
        }

        const auto& get_storage() const
        {
            return _storage;
        }
    };

    template <typename TValue, typename TAllocator>
    class List : public Linqable<List<TValue, TAllocator>>
    {
    public:
        using iterator = typename std::vector<TValue, TAllocator>::iterator;
        using const_iterator = typename std::vector<TValue, TAllocator>::const_iterator;

        /// Wraps std::vector with LINQ-style helpers.
        /// Purpose: exposes dynamic contiguous storage with the linqable surface while presenting a
        /// consistent C#-like API.
        /// Ownership: owns stored values.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        List() = default;

        /// Initializes the list from an initializer list.
        /// Purpose: enables concise creation mirroring C# collection initializers.
        /// Ownership: copies the provided values into owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        List(std::initializer_list<TValue> values)
            : _storage(values)
        {
        }

        /// Adds a value to the end of the list.
        /// Purpose: mirrors C# List Add semantics.
        /// Ownership: copies the provided value into owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        bool add(const TValue& value)
        {
            _storage.push_back(value);
            return true;
        }

        /// Emplaces a value at the end of the list.
        /// Purpose: constructs the stored value in place to avoid redundant copies.
        /// Ownership: owns the in-place constructed value.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        template <typename... TArgs>
        bool emplace(TArgs&&... args)
        {
            _storage.emplace_back(std::forward<TArgs>(args)...);
            return true;
        }

        /// Removes all items from the list.
        /// Purpose: resets the container to an empty state.
        /// Ownership: retains ownership of storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        void clear()
        {
            _storage.clear();
        }

        /// Retrieves the number of stored items.
        /// Purpose: reports the active item count for sizing and LINQ operations.
        /// Ownership: does not transfer ownership.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        uint get_count() const
        {
            return static_cast<uint>(_storage.size());
        }

        /// Retrieves a reference to the stored value at the provided index.
        /// Purpose: matches the C# indexer semantics while using explicit Get prefix.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        TValue& get_at(uint index)
        {
            return _storage.at(index);
        }

        /// Retrieves a const reference to the stored value at the provided index.
        /// Purpose: matches the C# indexer semantics while using explicit Get prefix.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        const TValue& get_at(uint index) const
        {
            return _storage.at(index);
        }

        /// Provides list-style access to stored values.
        /// Purpose: aligns with square bracket usage expectations similar to C# indexers.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        TValue& operator[](uint index)
        {
            return get_at(index);
        }

        /// Provides read-only list-style access to stored values.
        /// Purpose: aligns with square bracket usage expectations similar to C# indexers.
        /// Ownership: returns a reference to owned storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        const TValue& operator[](uint index) const
        {
            return get_at(index);
        }

        /// Combines two lists into a single concatenated collection.
        /// Purpose: supports C#-style additive composition by appending items from both inputs.
        /// Ownership: the returned list owns copied values; inputs retain ownership of their
        /// storage.
        /// Thread Safety: not thread-safe; callers must synchronize external access.
        List<TValue, TAllocator> operator+(const List<TValue, TAllocator>& other) const
        {
            List<TValue, TAllocator> combined;
            combined.add_range(this->begin(), this->end());
            combined.add_range(other.begin(), other.end());
            return combined;
        }

    private:
        friend class Linqable<List<TValue, TAllocator>>;

        template <typename TIterator>
        void add_range(TIterator begin_iterator, TIterator end_iterator)
        {
            for (auto iterator = begin_iterator; iterator != end_iterator; ++iterator)
            {
                add(*iterator);
            }
        }

        std::vector<TValue, TAllocator> _storage;

        auto& get_storage()
        {
            return _storage;
        }

        const auto& get_storage() const
        {
            return _storage;
        }
    };
}
