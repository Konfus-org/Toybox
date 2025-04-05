#pragma once
#include "Tbx/Core/Math/Int.h"
#include "Tbx/Core/Ids/UID.h"
#include "Tbx/Core/Callbacks/CallbackFunction.h"

namespace Tbx
{
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
        std::string _name;
        CallbackFunction<TArg> _callbackFn = nullptr;
        UID _id = UID();
    };
}
