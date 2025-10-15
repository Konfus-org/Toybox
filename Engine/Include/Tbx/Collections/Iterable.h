#pragma once
#include "Tbx/Math/Int.h"
#include <functional>
#include <algorithm>
#include <vector>

namespace Tbx
{
    template <typename TItem>
    class Iterable
    {
    public:
        using iterator = typename std::vector<TItem>::iterator;
        using const_iterator = typename std::vector<TItem>::const_iterator;
        using reverse_iterator = typename std::vector<TItem>::reverse_iterator;
        using const_reverse_iterator = typename std::vector<TItem>::const_reverse_iterator;

        Iterable() = default;
        Iterable(const std::vector<TItem>& items)
            : _items(items)
        {
        }

        bool Any() const
        {
            return !Empty();
        }

        bool Empty() const
        {
            return _items.empty();
        }

        uint64 Count() const
        {
            return _items.size();
        }

        const std::vector<TItem>& All() const
        {
            return _items;
        }

        const std::vector<TItem>& Vector() const
        {
            return _items;
        }

        const TItem& First() const
        {
            return _items.front();
        }

        template <typename TKeySelector, typename TCompare = std::less<>>
        std::vector<TItem> OrderBy(TKeySelector selector, TCompare compare = {}) const
        {
            std::vector<TItem> ordered = _items;
            std::sort(
                ordered.begin(),
                ordered.end(),
                [&](const TItem& lhs, const TItem& rhs)
                {
                    return compare(selector(lhs), selector(rhs));
                });
            return ordered;
        }

        iterator begin()
        {
            return _items.begin();
        }

        iterator end()
        {
            return _items.end();
        }

        const_iterator begin() const
        {
            return _items.begin();
        }

        const_iterator end() const
        {
            return _items.end();
        }

        const_iterator cbegin() const
        {
            return _items.cbegin();
        }

        const_iterator cend() const
        {
            return _items.cend();
        }

        reverse_iterator rbegin()
        {
            return _items.rbegin();
        }

        reverse_iterator rend()
        {
            return _items.rend();
        }

        const_reverse_iterator rbegin() const
        {
            return _items.rbegin();
        }

        const_reverse_iterator rend() const
        {
            return _items.rend();
        }

        const_reverse_iterator crbegin() const
        {
            return _items.crbegin();
        }

        const_reverse_iterator crend() const
        {
            return _items.crend();
        }

    protected:
        std::vector<TItem>& MutableItems()
        {
            return _items;
        }

    private:
        std::vector<TItem> _items = {};
    };
}
