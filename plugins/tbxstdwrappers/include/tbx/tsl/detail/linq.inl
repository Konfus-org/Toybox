#pragma once

namespace tbx::linq
{
    template <std::ranges::range Range>
    inline void reserve_if_possible(List<std::ranges::range_value_t<Range>>& values, const Range& range)
    {
        if constexpr (std::ranges::sized_range<Range>)
        {
            values.reserve(static_cast<uint>(std::ranges::size(range)));
        }
    }

    template <std::ranges::input_range Range>
    inline List<std::ranges::range_value_t<Range>> to_list(const Range& range)
    {
        List<std::ranges::range_value_t<Range>> result;
        reserve_if_possible(result, range);
        for (auto&& value : range)
        {
            result.emplace_back(value);
        }
        return result;
    }

    template <std::ranges::input_range Range>
    inline List<std::ranges::range_value_t<Range>> to_vector(const Range& range)
    {
        return to_list(range);
    }

    template <std::ranges::input_range Range, typename Projection>
    inline auto select(const Range& range, Projection&& projection)
    {
        using Result = Decayed<std::invoke_result_t<Projection&, std::ranges::range_reference_t<Range>>>;
        List<Result> result;
        reserve_if_possible(result, range);
        for (auto&& value : range)
        {
            result.emplace_back(std::invoke(projection, value));
        }
        return result;
    }

    template <std::ranges::input_range Range, typename Predicate>
    inline List<std::ranges::range_value_t<Range>> where(const Range& range, Predicate&& predicate)
    {
        List<std::ranges::range_value_t<Range>> result;
        for (auto&& value : range)
        {
            if (std::invoke(predicate, value))
            {
                result.emplace_back(value);
            }
        }
        return result;
    }

    template <std::ranges::input_range Range>
    inline std::ranges::range_value_t<Range> first(const Range& range)
    {
        using Value = std::ranges::range_value_t<Range>;
        for (auto&& value : range)
        {
            return static_cast<Value>(value);
        }
        throw std::out_of_range("tbx::linq::first: range is empty");
    }

    template <std::ranges::input_range Range, typename Predicate>
    inline std::ranges::range_value_t<Range> first(const Range& range, Predicate&& predicate)
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
    inline TValue first_or_default(const Range& range, TValue default_value)
    {
        for (auto&& value : range)
        {
            return static_cast<TValue>(value);
        }
        return default_value;
    }

    template <std::ranges::input_range Range, typename Predicate, typename TValue>
    inline TValue first_or_default(const Range& range, Predicate&& predicate, TValue default_value)
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
    inline bool any(const Range& range, Predicate&& predicate)
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
    inline bool any(const Range& range)
    {
        for (auto&& value : range)
        {
            (void)value;
            return true;
        }
        return false;
    }

    template <std::ranges::input_range Range, typename Predicate>
    inline bool all(const Range& range, Predicate&& predicate)
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
    inline uint count(const Range& range)
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
    inline uint count(const Range& range, Predicate&& predicate)
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
    inline bool contains(const Range& range, const TValue& needle)
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
