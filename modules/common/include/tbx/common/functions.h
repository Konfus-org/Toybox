#pragma once
#include "tbx/common/string.h"
#include "tbx/common/uuid.h"
#include <functional>
#include <type_traits>

namespace tbx
{
    template <typename... TArgs>
    using CallbackFunction = std::function<void(TArgs...)>;

    template <typename... TArgs>
    class Callback
    {
      public:
        Callback() = default;

        Callback(CallbackFunction<TArgs...> func, String name = {})
            : _callback(std::move(func))
            , _name(std::move(name))
            , _id(Uuid::generate())
        {
        }

        void reset(CallbackFunction<TArgs...> func, String name = {})
        {
            _callback = std::move(func);
            _name = std::move(name);
            _id = Uuid::generate();
        }

        void invoke(TArgs... args) const
        {
            if (_callback)
                _callback(std::forward<TArgs>(args)...);
        }

        void operator()(TArgs... args) const
        {
            invoke(std::forward<TArgs>(args)...);
        }

        explicit operator bool() const
        {
            return static_cast<bool>(_callback);
        }

        const String& name() const
        {
            return _name;
        }

        const Uuid& id() const
        {
            return _id;
        }

      private:
        CallbackFunction<TArgs...> _callback = nullptr;
        String _name;
        Uuid _id = Uuid::generate();
    };

    template <typename T>
    struct FunctionTraits;

    template <typename R, typename C, typename Arg>
    struct FunctionTraits<R (C::*)(Arg) const>
    {
        using ArgType = Arg;
    };

    template <typename R, typename C, typename Arg>
    struct FunctionTraits<R (C::*)(Arg)>
    {
        using ArgType = Arg;
    };
}
