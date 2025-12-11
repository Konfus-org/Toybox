#pragma once
#include "tbx/common/result.h"
#include "tbx/tbx_api.h"
#include <optional>

namespace tbx
{
    template <typename TValue>
    class PayloadResult : public Result
    {
      public:
        PayloadResult() = default;

        bool has_payload() const
        {
            return _payload.has_value();
        }

        const TValue& get_payload() const
        {
            return *_payload;
        }

        void set_payload(TValue value)
        {
            _payload = value;
        }

        void reset_payload()
        {
            _payload.reset();
        }

      private:
        std::optional<TValue> _payload;
    };
}
