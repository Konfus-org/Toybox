#pragma once
#include <type_traits>

namespace tbx
{
    // Primary template left undefined
    template <typename T>
    struct FunctionTraits;

    // Lambdas / functors: operator()(Arg) const
    template <typename R, typename C, typename Arg>
    struct FunctionTraits<R (C::*)(Arg) const>
    {
        using ArgType = Arg;
    };

    // Non-const operator() just in case
    template <typename R, typename C, typename Arg>
    struct FunctionTraits<R (C::*)(Arg)>
    {
        using ArgType = Arg;
    };
}
