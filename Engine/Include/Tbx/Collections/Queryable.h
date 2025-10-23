#pragma once
#include "Tbx/Collections/Iterable.h"
#include "Tbx/Memory/Refs.h"
#include <type_traits>
#include <vector>

#pragma once
#include <type_traits>
#include <concepts>
#include <vector>
#include <algorithm>
#include <utility>
#include <functional> // std::invoke

namespace Tbx
{
    /// <summary>
    /// Concept that matches lambda-like callables (closure objects with operator()).
    /// This intentionally excludes raw function pointers.
    /// </summary>
    template <class T>
    concept IsLambda =
        std::is_class_v<std::remove_reference_t<T>> &&
        requires { &std::remove_reference_t<T>::operator(); };

    /// <summary>
    /// Provides LINQ-like sequence operators over a contiguous collection of items.
    /// </summary>
    /// <typeparam name="TItem">Element type.</typeparam>
    template <typename TItem>
    class Queryable : public Iterable<TItem>
    {
    public:
        /// <summary>
        /// Initializes an empty queryable sequence.
        /// </summary>
        Queryable() = default;

        /// <summary>
        /// Initializes a queryable sequence from an existing vector of items.
        /// </summary>
        /// <param name="items">Items to wrap.</param>
        Queryable(const std::vector<TItem>& items)
            : Iterable<TItem>(items)
        {
        }

        /// <summary>
        /// Returns true if the sequence contains any elements.
        /// </summary>
        bool Any() const
        {
            return Iterable<TItem>::Any();
        }

        /// <summary>
        /// Returns true if any element satisfies <paramref name="predicate"/>.
        /// </summary>
        /// <typeparam name="TPredicate">Callable predicate type.</typeparam>
        /// <param name="predicate">Predicate to test each element.</param>
        /// <returns>True if any element matches; otherwise false.</returns>
        template <typename TPredicate>
        bool Any(TPredicate predicate) const
        {
            const auto& items = this->All();
            return std::any_of(items.begin(), items.end(), predicate);
        }

        /// <summary>
        /// Returns the first element of the sequence; if the sequence is empty, returns a default value.
        /// </summary>
        /// <returns>First element or a default-initialized sentinel.</returns>
        const TItem& First() const
        {
            const auto& items = this->All();
            if (!items.empty())
            {
                return items.front();
            }

            return _default;
        }

        /// <summary>
        /// Returns the first element that satisfies <paramref name="predicate"/>; if none match, returns a default value.
        /// </summary>
        /// <typeparam name="TPredicate">Callable predicate type.</typeparam>
        /// <param name="predicate">Predicate to test each element.</param>
        /// <returns>Matching element or a default-initialized sentinel.</returns>
        template <typename TPredicate>
        const TItem& First(TPredicate predicate) const
        {
            const auto& items = this->All();
            const auto it = std::find_if(items.begin(), items.end(), predicate);
            if (it != items.end())
            {
                return *it;
            }

            return _default;
        }

        /// <summary>
        /// Projects each element of the sequence into a new form using <paramref name="selector"/>.
        /// </summary>
        /// <typeparam name="TSelector">Lambda-like callable that accepts <c>const TItem&amp;</c>.</typeparam>
        /// <param name="selector">Projection function.</param>
        /// <returns>A new <c>Queryable&lt;Out&gt;</c> where Out is the selector's return type.</returns>
        template <typename TSelector>
        requires std::invocable<TSelector, const TItem&>&& IsLambda<TSelector>
        auto Select(TSelector&& selector) const
            -> Queryable<std::invoke_result_t<TSelector, const TItem&>>
        {
            using Out = std::invoke_result_t<TSelector, const TItem&>;

            const auto& items = this->All();

            std::vector<Out> result = {};
            result.reserve(items.size());

            for (const auto& item : items)
            {
                result.push_back(std::invoke(selector, item));
            }

            return Queryable<Out>(result);
        }

        /// <summary>
        /// Filters the sequence to elements that satisfy <paramref name="predicate"/>.
        /// </summary>
        /// <typeparam name="TPredicate">Callable predicate type.</typeparam>
        /// <param name="predicate">Predicate to test each element.</param>
        /// <returns>A new <c>Queryable&lt;TItem&gt;</c> containing matching elements.</returns>
        template <typename TPredicate>
        auto Where(TPredicate predicate) const
        {
            const auto& items = this->All();

            std::vector<TItem> result = {};
            result.reserve(items.size());

            for (const auto& item : items)
            {
                if (predicate(item))
                {
                    result.push_back(item);
                }
            }

            return Queryable<TItem>(result);
        }

        /// <summary>
        /// Filters the sequence to elements that can be dynamically cast to <typeparamref name="TDerived"/>.
        /// </summary>
        /// <typeparam name="TDerived">Target derived type.</typeparam>
        /// <returns>A queryable sequence of successfully cast elements.</returns>
        template <typename TDerived>
        auto OfType() const
        {
            const auto& items = this->All();

            if constexpr (std::is_pointer_v<TItem>)
            {
                std::vector<TDerived*> result = {};
                for (auto p : items)
                {
                    if (auto q = dynamic_cast<TDerived*>(p))
                    {
                        result.push_back(q);
                    }
                }
                return Queryable<TDerived*>(result);
            }
            else if constexpr (IsExclusiveRef<TItem>::value)
            {
                std::vector<TDerived*> result = {};
                for (auto& item : items)
                {
                    if (TDerived* casted = dynamic_cast<TDerived*>(item.get()))
                    {
                        result.push_back(casted);
                    }
                }
                return Queryable<TDerived*>(result);
            }
            else if constexpr (IsWeakRef<TItem>::value)
            {
                std::vector<WeakRef<TDerived>> result = {};
                for (auto& item : items)
                {
                    if (Ref<TDerived> casted = std::dynamic_pointer_cast<TDerived>(item.lock()))
                    {
                        result.push_back(WeakRef<TDerived>(casted));
                    }
                }
                return Queryable<WeakRef<TDerived>>(result);
            }
            else if constexpr (IsRef<TItem>::value)
            {
                std::vector<Ref<TDerived>> result = {};
                for (auto& item : items)
                {
                    if (Ref<TDerived> casted = std::dynamic_pointer_cast<TDerived>(item))
                    {
                        result.push_back(casted);
                    }
                }
                return Queryable<Ref<TDerived>>(result);
            }
            else
            {
                static_assert(!sizeof(TItem), "Queryable::OfType requires pointer, Ref, WeakRef, or ExclusiveRef items");
            }
        }

    private:
        /// <summary>
        /// Default value returned when a query cannot produce an element (e.g., First on an empty sequence).
        /// </summary>
        TItem _default = {};
    };
}
