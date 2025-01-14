#pragma once
#include "Math/Int.h"
#include "Util/UUID.h"

namespace Tbx
{
    template <typename TArg>
    using CallbackFunction = std::function<void(TArg&)>;

    template <typename TArg>
    class Callback
    {
    public:
        explicit(false) Callback(CallbackFunction<TArg> func)
            : _callbackFn(std::move(func)), _id(UUID::Generate()) {}

        const UUID& GetId() const
        {
            return _id;
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
        CallbackFunction<TArg> _callbackFn;
        UUID _id;
    };
}
