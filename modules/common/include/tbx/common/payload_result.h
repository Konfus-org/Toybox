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

        bool has_payload() const;

        const TValue& get_payload() const;

        void set_payload(TValue value);

        void reset_payload();

      private:
        std::optional<TValue> _payload;
    };
}

#include "tbx/common/payload_result.inl"
