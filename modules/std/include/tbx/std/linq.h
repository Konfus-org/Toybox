#pragma once
#include "tbx/std/list.h"
#include "tbx/std/int.h"
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
    void reserve_if_possible(List<std::ranges::range_value_t<Range>>& values, const Range& range);

    template <std::ranges::input_range Range>
    List<std::ranges::range_value_t<Range>> to_list(const Range& range);

    template <std::ranges::input_range Range>
    List<std::ranges::range_value_t<Range>> to_vector(const Range& range);

    template <std::ranges::input_range Range, typename Projection>
    auto select(const Range& range, Projection&& projection);

    template <std::ranges::input_range Range, typename Predicate>
    List<std::ranges::range_value_t<Range>> where(const Range& range, Predicate&& predicate);

    template <std::ranges::input_range Range>
    std::ranges::range_value_t<Range> first(const Range& range);

    template <std::ranges::input_range Range, typename Predicate>
    std::ranges::range_value_t<Range> first(const Range& range, Predicate&& predicate);

    template <std::ranges::input_range Range, typename TValue>
    TValue first_or_default(const Range& range, TValue default_value);

    template <std::ranges::input_range Range, typename Predicate, typename TValue>
    TValue first_or_default(const Range& range, Predicate&& predicate, TValue default_value);

    template <std::ranges::input_range Range, typename Predicate>
    bool any(const Range& range, Predicate&& predicate);

    template <std::ranges::input_range Range>
    bool any(const Range& range);

    template <std::ranges::input_range Range, typename Predicate>
    bool all(const Range& range, Predicate&& predicate);

    template <std::ranges::input_range Range>
    uint count(const Range& range);

    template <std::ranges::input_range Range, typename Predicate>
    uint count(const Range& range, Predicate&& predicate);

    template <std::ranges::input_range Range, typename TValue>
    bool contains(const Range& range, const TValue& needle);
}

#include "../../../src/std/linq.inl"
