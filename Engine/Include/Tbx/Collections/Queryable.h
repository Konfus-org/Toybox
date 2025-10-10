#pragma once
#include "Tbx/Collections/Iterable.h"
#include "Tbx/Memory/Refs.h"
#include <type_traits>
#include <vector>

namespace Tbx
{
    template <typename TItem>
    class Queryable : public Iterable<TItem>
    {
    public:
        Queryable() = default;
        Queryable(const std::vector<TItem>& items)
            : Iterable<TItem>(items)
        {
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
        std::vector<TItem&> Where(TPredicate predicate) const
        {
            std::vector<TItem> result = {};
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
        const TItem& First(TPredicate predicate) const
        {
            const auto& items = this->All();
            const auto it = std::find_if(items.begin(), items.end(), predicate);
            if (it != items.end())
            {
                return *it;
            }

            return {};
        }

        template <typename TDerived>
        auto OfType() const
        {
            const auto& items = this->All();

            if constexpr (std::is_pointer<TItem>::value)
            {
                std::vector<TDerived*> result = {};
                for (auto p : items)
                {
                    if (auto q = dynamic_cast<TDerived*>(p))
                    {
                        result.push_back(q);
                    }
                }
                return result;
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
                return result;
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
                return result;
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
                return result;
            }
            else
            {
                static_assert(!sizeof(TItem), "Queryable::OfType requires pointer, Ref, WeakRef, or ExclusiveRef items");
            }
        }
    };
}
