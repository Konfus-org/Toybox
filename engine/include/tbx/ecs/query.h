#pragma once
#include "tbx/api.h"
#include "tbx/ecs/registry.h" // Registry/ToyId — toy.h uses them but leaves the seam to its includer
#include "tbx/ecs/toy.h"
#include "tbx/platform/window.h"
#include "tbx/reflection/type_registry.h"     // TypeInfo + describe_type<T>
#include "tbx/serialization/registry.h"       // SerializerInfo + describe_serializer<T>
#include "tbx/utils/typedefs.h"
#include <concepts>
#include <functional>
#include <optional>
#include <vector>

// The one consistent way to enumerate "all of X" in the running runtime, no matter what X is:
// get_all<Toy>(), get_all<Window>(), … each return a Query<T> you filter fluently
// (.where(...).with<Components...>()) and drain (.first()/.count()/range-for/to_vector()). A new
// queryable type is one QuerySource<T> specialization — nothing else changes. Main-thread only
// (the sources read tbx::internal::get_runtime()).
namespace tbx
{
    /// @brief
    /// Purpose: How get_all<T>() enumerates every T from the running runtime. The primary is
    /// intentionally undefined so an unsupported T is a clean compile error (see Queryable); each
    /// supported type specializes it. `element` is what the Query stores per item: an owned handle
    /// for cheap value types (Toy), or a reference for non-copyable ones (Window) — either way it
    /// converts to `const T&`, so the Query speaks one vocabulary.
    template <typename T>
    struct QuerySource;

    template <>
    struct TBX_DLL_EXPORT QuerySource<Toy>
    {
        using element = Toy;
        static std::vector<element> collect();
    };

    template <>
    struct TBX_DLL_EXPORT QuerySource<Window>
    {
        using element = std::reference_wrapper<const Window>;
        static std::vector<element> collect();
    };

    template <>
    struct TBX_DLL_EXPORT QuerySource<TypeInfo>
    {
        using element = std::reference_wrapper<const TypeInfo>;
        static std::vector<element> collect(); // the reflection registry (process-global, no runtime)
    };

    template <>
    struct TBX_DLL_EXPORT QuerySource<SerializerInfo>
    {
        using element = std::reference_wrapper<const SerializerInfo>;
        static std::vector<element> collect(); // the serializer registry (process-global, no runtime)
    };

    /// @brief
    /// Purpose: A type get_all<T>() supports — i.e. one with a QuerySource<T> specialization.
    template <typename T>
    concept Queryable = requires { typename QuerySource<T>::element; };

    /// @brief
    /// Purpose: Iterates a Query<T> as `const T&`, unwrapping the stored element (so a Query<Window>
    /// yields const Window&, not the reference_wrapper it stores). A namespace-level type (not
    /// nested) so it composes with range-for without breaking the no-nested-types rule.
    template <Queryable T>
    class QueryIterator final
    {
      public:
        using inner = typename std::vector<typename QuerySource<T>::element>::const_iterator;

        explicit QueryIterator(inner position)
            : _position(position)
        {
        }

        const T& operator*() const
        {
            return *_position;
        }
        QueryIterator& operator++()
        {
            ++_position;
            return *this;
        }
        bool operator!=(const QueryIterator& other) const
        {
            return _position != other._position;
        }

      private:
        inner _position = {};
    };

    /// @brief
    /// Purpose: A chainable snapshot of the T's get_all<T>() collected — filter it in place
    /// (where/with) then drain it (first/count/range-for/to_vector). A same-frame, main-thread view:
    /// mutating the world while iterating one invalidates it, exactly like get_toys()/get_open_windows().
    template <Queryable T>
    class Query final
    {
      public:
        using element = typename QuerySource<T>::element;

        explicit Query(std::vector<element> items)
            : _items(std::move(items))
        {
        }

        /// @brief
        /// Purpose: Keeps only the items the predicate accepts. Fluent.
        Query& where(const std::function<bool(const T&)>& predicate)
        {
            std::erase_if(_items, [&](const element& item) { return !predicate(item); });
            return *this;
        }

        /// @brief
        /// Purpose: Toys only — keeps only the toys carrying all of TBlocks. Fluent. Constrained to
        /// Toy so get_all<Window>().with<...>() is a compile error, not a silent no-op.
        template <typename... TBlocks>
            requires std::same_as<T, Toy>
        Query& with()
        {
            std::erase_if(_items, [](const Toy& toy) { return !(toy.has<TBlocks>() && ...); });
            return *this;
        }

        /// @brief
        /// Purpose: How many items remain.
        size count() const
        {
            return _items.size();
        }

        /// @brief
        /// Purpose: True when no items remain.
        bool is_empty() const
        {
            return _items.empty();
        }

        /// @brief
        /// Purpose: The first item, or nothing — for copyable T (Toy), returned by value.
        std::optional<T> first() const
            requires std::copyable<T>
        {
            if (_items.empty())
                return {};
            return _items.front();
        }

        /// @brief
        /// Purpose: The first item, or nullptr — for non-copyable T (Window), returned by pointer
        /// (a Window cannot be handed back by value).
        const T* first() const
            requires(!std::copyable<T>)
        {
            return _items.empty() ? nullptr : &static_cast<const T&>(_items.front());
        }

        /// @brief
        /// Purpose: The remaining items as a plain vector — copyable T only.
        std::vector<T> to_vector() const
            requires std::copyable<T>
        {
            return std::vector<T>(_items.begin(), _items.end());
        }

        /// @brief
        /// Purpose: Implicit convert-to-vector so a Query<T> drops straight into a std::vector<T> —
        /// copyable T only.
        operator std::vector<T>() const
            requires std::copyable<T>
        {
            return to_vector();
        }

        QueryIterator<T> begin() const
        {
            return QueryIterator<T>(_items.begin());
        }
        QueryIterator<T> end() const
        {
            return QueryIterator<T>(_items.end());
        }

      private:
        std::vector<element> _items;
    };

    /// @brief
    /// Purpose: Every T as a chainable Query — get_all<Toy>(), get_all<Window>(),
    /// get_all<TypeInfo>(), get_all<SerializerInfo>(), … World-backed sources are main-thread only;
    /// registry-backed ones (TypeInfo/SerializerInfo) need no runtime.
    template <Queryable T>
    Query<T> get_all()
    {
        return Query<T>(QuerySource<T>::collect());
    }

    /// @brief
    /// Purpose: The toys carrying all of TBlocks — get_with<Toy, Transform, RigidBody>(). Shorthand
    /// for get_all<Toy>().with<TBlocks...>(). Toy-only (that is what carries blocks).
    template <typename T, typename... TBlocks>
        requires std::same_as<T, Toy>
    Query<Toy> get_with()
    {
        auto query = get_all<Toy>();
        query.template with<TBlocks...>();
        return query;
    }

    /// @brief
    /// Purpose: The T's matching a predicate — get_where<Window>(pred), get_where<Toy>(pred),
    /// get_where<TypeInfo>(pred). Shorthand for get_all<T>().where(pred).
    template <Queryable T>
    Query<T> get_where(const std::function<bool(const T&)>& predicate)
    {
        auto query = get_all<T>();
        query.where(predicate);
        return query;
    }

    /// @brief
    /// Purpose: The reflection descriptor for a C++ type, or nothing — get_type<Toy>(),
    /// get_type<Material>(). The single-item counterpart to get_all<TypeInfo>().
    template <typename T>
    std::optional<std::reference_wrapper<const TypeInfo>> get_type()
    {
        return describe_type<T>();
    }

    /// @brief
    /// Purpose: The serializer descriptor for a C++ type, or nothing — get_serializer<Kit>(),
    /// get_serializer<App>(). The single-item counterpart to get_all<SerializerInfo>().
    template <typename T>
    std::optional<std::reference_wrapper<const SerializerInfo>> get_serializer()
    {
        return describe_serializer<T>();
    }
}
