#pragma once
#include "Tbx/Collections/Queryable.h"

namespace Tbx
{
    template <typename TItem>
    class Collection : public Queryable<TItem>
    {
    public:
        Collection() = default;
        Collection(const std::vector<TItem>& items)
            : Queryable<TItem>(items)
        {
        }

        template <typename... TArgs>
        void Emplace(TArgs&&... args)
        {
            auto item = TItem(std::forward<TArgs>(args)...);
            Add(std::move(item));
        }

        void Add(const TItem& item)
        {
            this->MutableItems().push_back(std::move(item));
        }

        void Clear()
        {
            this->MutableItems().clear();
        }

        bool Remove(const TItem& item)
        {
            const auto it = std::find(this->MutableItems().begin(), this->MutableItems().end(), item);
            if (it == this->MutableItems().end())
            {
                return false;
            }

            this->MutableItems().erase(it);
            return true;
        }

        template <typename TPredicate>
        uint64 RemoveAll(TPredicate predicate)
        {
            uint64 removed = 0;
            auto it = this->MutableItems().begin();
            while (it != this->MutableItems().end())
            {
                if (predicate(*it))
                {
                    it = this->MutableItems().erase(it);
                    ++removed;
                    continue;
                }

                ++it;
            }

            return removed;
        }
    };
}
