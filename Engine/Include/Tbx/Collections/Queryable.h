#pragma once
#include "Tbx/Collections/Iterable.h"
#include "Tbx/Memory/Refs.h"
#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

namespace Tbx
{
    template <typename TItem>
    class Queryable : public Iterable<TItem>
    {
    public:
        using Iterable<TItem>::Iterable;
        using typename Iterable<TItem>::Container;

        bool Any() const
        {
            return !this->Empty();
        }

        template <typename TPredicate>
        bool Any(TPredicate predicate) const
        {
            for (const auto& item : this->All())
            {
                if (predicate(item))
                {
                    return true;
                }
            }

            return false;
        }

        template <typename TPredicate>
        Container Where(TPredicate predicate) const
        {
            Container result;
            const auto& items = this->All();
            result.reserve(items.size());

            for (const auto& item : items)
            {
                if (predicate(item))
                {
                    result.push_back(item);
                }
            }

            return result;
        }

        template <typename TPredicate>
        TItem First(TPredicate predicate) const
        {
            const auto& items = this->All();
            const auto it = std::find_if(items.begin(), items.end(), predicate);
            if (it != items.end())
            {
                return *it;
            }

            return TItem{};
        }

        TItem First() const
        {
            const auto& items = this->All();
            if (!items.empty())
            {
                return items.front();
            }

            return TItem{};
        }

        template <typename TDerived>
        auto OfType() const
        {
            return OfTypeImpl<TDerived>(std::integral_constant<bool, std::is_pointer_v<TItem>>{});
        }

    private:
        template <typename TDerived>
        auto OfTypeImpl(std::true_type) const
        {
            using ResultType = TDerived*;
            std::vector<ResultType> result;
            const auto& items = this->All();
            result.reserve(items.size());

            for (auto item : items)
            {
                if (auto casted = dynamic_cast<ResultType>(item))
                {
                    result.push_back(casted);
                }
            }

            return result;
        }

        template <typename TDerived>
        auto OfTypeImpl(std::false_type) const
        {
            return OfTypeRef<TDerived>(std::integral_constant<bool, IsRef<TItem>::value>{});
        }

        template <typename TDerived>
        auto OfTypeRef(std::true_type) const
        {
            std::vector<Ref<TDerived>> result;
            const auto& items = this->All();
            result.reserve(items.size());

            for (const auto& item : items)
            {
                if (item == nullptr)
                {
                    continue;
                }

                if (auto casted = std::dynamic_pointer_cast<TDerived>(item))
                {
                    result.push_back(std::move(casted));
                }
            }

            return result;
        }

        template <typename TDerived>
        auto OfTypeRef(std::false_type) const
        {
            static_assert(sizeof(TDerived) == 0, "Queryable::OfType requires pointer or ref types");
            return std::vector<TDerived>{};
        }
    };
}
