#pragma once
#include "tbx/common/functions.h"
#include "tbx/common/payload_result.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include "tbx/time/time_span.h"
#include <atomic>
#include <chrono>
#include <future>
#include <type_traits>
#include <utility>

namespace tbx
{
    template <typename TValue>
    class Promise
    {
      public:
        Promise() = default;

        PayloadResult<TValue> resolve() const
        {
            return current_result();
        }

        PayloadResult<TValue> wait() const
        {
            auto& state = ensure_state();
            state.future.wait();
            return current_result();
        }

        PayloadResult<TValue> wait(TimeSpan timeout_after) const
        {
            auto& state = ensure_state();

            if (timeout_after.is_zero()
                && state.future.wait_for(std::chrono::steady_clock::duration::zero())
                       != std::future_status::ready)
            {
                return {};
            }
            else if (
                state.future.wait_for(timeout_after.to_duration()) != std::future_status::ready)
            {
                return {};
            }

            return current_result();
        }

        void fulfill(TValue value)
        {
            auto& state = ensure_state();
            if (state.future.wait_for(std::chrono::steady_clock::duration::zero())
                == std::future_status::ready)
            {
                return;
            }

            PayloadResult<TValue> payload;
            if constexpr (std::is_same_v<TValue, Result>)
            {
                const bool succeeded = value.succeeded();
                const String& report = value.get_report();
                payload.set_payload(std::move(value));
                if (succeeded)
                    payload.flag_success(report);
                else
                    payload.flag_failure(report);
            }
            else
            {
                payload.set_payload(std::move(value));
                payload.flag_success();
            }

            try
            {
                state.promise.set_value(std::move(payload));
            }
            catch (const std::future_error& err)
            {
                fail(err.what());
                return;
            }

            notify_ready();
        }

        void fail(String reason)
        {
            auto& state = ensure_state();
            if (state.future.wait_for(std::chrono::steady_clock::duration::zero())
                == std::future_status::ready)
            {
                return;
            }

            PayloadResult<TValue> payload;
            payload.reset_payload();
            payload.flag_failure(std::move(reason));

            try
            {
                state.promise.set_value(std::move(payload));
            }
            catch (const std::future_error&)
            {
                return;
            }

            notify_ready();
        }

        bool is_ready() const
        {
            auto& state = ensure_state();
            return state.future.wait_for(std::chrono::steady_clock::duration::zero())
                   == std::future_status::ready;
        }

      public:
        Callback<const Result&> on_failure;
        Callback<const TValue&> on_ready;

      private:
        struct State
        {
            State()
                : future(promise.get_future().share())
            {
            }

            std::promise<PayloadResult<TValue>> promise;
            std::shared_future<PayloadResult<TValue>> future;
        };

        State& ensure_state() const
        {
            if (!_state)
                _state = Ref<State>(new State());
            return *_state;
        }

        PayloadResult<TValue> current_result() const
        {
            return ensure_state().future.get();
        }

        void notify_ready()
        {
            auto payload = current_result();
            if (payload.succeeded())
                invoke_on_ready(payload);
            else
                invoke_on_failure(payload);
        }

        void invoke_on_ready(const PayloadResult<TValue>& payload) const
        {
            if (!on_ready || !payload.has_payload())
                return;

            on_ready(payload.get_payload());
        }

        void invoke_on_failure(const PayloadResult<TValue>& payload) const
        {
            if (on_failure)
                on_failure(payload);
        }

        mutable Ref<State> _state;
    };
}
