#pragma once
#include "Tbx/Utils/Ids/UsesUID.h"
#include "Tbx/Utils/Callbacks/CallbackFunction.h"

#define TBX_BIND_FN(fn) [this](auto&&... args) { return this->fn(std::forward<decltype(args)>(args)...); }
#define TBX_BIND_STATIC_FN(fn) [](auto&&... args) { return fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    /// <summary>
    /// A wrapper around a method/function pointer that allows us to store a function and call it later.
    /// If passing a classes function you must first bind it to the callback like using TBX_BIND_FN or if the function is static use TBX_BIND_STATIC_FN.
    /// </summary>
    template <typename TArg>
    class Callback : public UsesUID
    {
    public:
        explicit(false) Callback(CallbackFunction<TArg> func)
            : _callbackFn(func), _name(func.target_type().name()) {}

        void Invoke(TArg& event) const { _callbackFn(event); }
        void operator()(TArg& event) const { Invoke(event); }

        std::string ToString() const { return _name; }

    private:
        CallbackFunction<TArg> _callbackFn = nullptr;
        std::string _name = "";
    };
}
