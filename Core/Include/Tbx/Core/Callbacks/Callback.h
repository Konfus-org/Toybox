#pragma once
#include "Tbx/Core/Math/Int.h"
#include "Tbx/Core/Ids/UID.h"
#include "Tbx/Core/Callbacks/CallbackFunction.h"

#define TBX_BIND_FN(fn) [this](auto&&... args) { return this->fn(std::forward<decltype(args)>(args)...); }
#define TBX_BIND_STATIC_FN(fn) [](auto&&... args) { return fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    /// <summary>
    /// A wrapper around a method/function pointer that allows us to store a function and call it later.
    /// If passing a classes function you must first bind it to the callback like using TBX_BIND_FN or if the function is static use TBX_BIND_STATIC_FN.
    /// </summary>
    template <typename TArg>
    class Callback
    {
    public:
        explicit(false) Callback(CallbackFunction<TArg> func)
            : _callbackFn(func), _name(func.target_type().name()) {}

        const UID& GetId() const
        {
            return _id;
        }

        const std::string& GetName() const
        {
            return _name;
        }

        void Invoke(TArg& event) const
        {
            _callbackFn(event);
        }

        void operator()(TArg& event) const
        {
            Invoke(event);
        }

        bool operator==(const Callback& other) const
        {
            return _id == other._id;
        }

    private:
        CallbackFunction<TArg> _callbackFn = nullptr;
        std::string _name = "";
        UID _id = UID();
    };
}
