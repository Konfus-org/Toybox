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
    };
}
