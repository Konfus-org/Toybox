#pragma once
#include "tbx/tsl/list.h"
#include "tbx/tsl/int.h"
#include <cstddef>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace tbx::linq
{
    template <typename TResult>
    using Decayed = std::decay_t<TResult>;

    template <std::ranges::range Range>
    static void reserve_if_possible(List<std::ranges::range_value_t<Range>>& values, const Range& range)
    {
        if constexpr (std::ranges::sized_range<Range>)
        {
            values.set_capacity(static_cast<uint>(std::ranges::size(range)));
        }
    }

    template <std::ranges::input_range Range>
    List<std::ranges::range_value_t<Range>> to_list(const Range& range)
    {
        List<std::ranges::range_value_t<Range>> result;
        reserve_if_possible(result, range);
        for (auto&& value : range)
        {
            result.set_emplace_back(value);
        }
        return result;
    }

    template <std::ranges::input_range Range>
    List<std::ranges::range_value_t<Range>> to_vector(const Range& range)
    {
        return to_list(range);
    }

    template <std::ranges::input_range Range, typename Projection>
    auto select(const Range& range, Projection&& projection)
    {
        using Result = Decayed<std::invoke_result_t<Projection&, std::ranges::range_reference_t<Range>>>;
        List<Result> result;
        reserve_if_possible(result, range);
        for (auto&& value : range)
        {
            result.set_emplace_back(std::invoke(projection, value));
        }
        return result;
    }

    template <std::ranges::input_range Range, typename Predicate>
    List<std::ranges::range_value_t<Range>> where(const Range& range, Predicate&& predicate)
    {
        List<std::ranges::range_value_t<Range>> result;
        for (auto&& value : range)
        {
            if (std::invoke(predicate, value))
            {
                result.set_emplace_back(value);
            }
        }
        return result;
    }

    template <std::ranges::input_range Range>
    std::ranges::range_value_t<Range> first(const Range& range)
    {
        using Value = std::ranges::range_value_t<Range>;
        for (auto&& value : range)
        {
            return static_cast<Value>(value);
        }
        throw std::out_of_range("tbx::linq::first: range is empty");
    }

    template <std::ranges::input_range Range, typename Predicate>
    std::ranges::range_value_t<Range> first(const Range& range, Predicate&& predicate)
    {
        using Value = std::ranges::range_value_t<Range>;
        for (auto&& value : range)
        {
            if (std::invoke(predicate, value))
            {
                return static_cast<Value>(value);
            }
        }
        throw std::out_of_range("tbx::linq::first: predicate did not match any item");
    }

    template <std::ranges::input_range Range, typename TValue>
    TValue first_or_default(const Range& range, TValue default_value)
    {
        for (auto&& value : range)
        {
            return static_cast<TValue>(value);
        }
        return default_value;
    }

    template <std::ranges::input_range Range, typename Predicate, typename TValue>
    TValue first_or_default(const Range& range, Predicate&& predicate, TValue default_value)
    {
        for (auto&& value : range)
        {
            if (std::invoke(predicate, value))
            {
                return static_cast<TValue>(value);
            }
        }
        return default_value;
    }

    template <std::ranges::input_range Range, typename Predicate>
    bool any(const Range& range, Predicate&& predicate)
    {
        for (auto&& value : range)
        {
            if (std::invoke(predicate, value))
            {
                return true;
            }
        }
        return false;
    }

    template <std::ranges::input_range Range>
    bool any(const Range& range)
    {
        for (auto&& value : range)
        {
            (void)value;
            return true;
        }
        return false;
    }

    template <std::ranges::input_range Range, typename Predicate>
    bool all(const Range& range, Predicate&& predicate)
    {
        for (auto&& value : range)
        {
            if (!std::invoke(predicate, value))
            {
                return false;
            }
        }
        return true;
    }

    template <std::ranges::input_range Range>
    uint count(const Range& range)
    {
        uint total = 0;
        for (auto&& value : range)
        {
            (void)value;
            ++total;
        }
        return total;
    }

    template <std::ranges::input_range Range, typename Predicate>
    uint count(const Range& range, Predicate&& predicate)
    {
        uint total = 0;
        for (auto&& value : range)
        {
            if (std::invoke(predicate, value))
            {
                ++total;
            }
        }
        return total;
    }

    template <std::ranges::input_range Range, typename TValue>
    bool contains(const Range& range, const TValue& needle)
    {
        for (auto&& value : range)
        {
            if (value == needle)
            {
                return true;
            }
        }
        return false;
    }
}
