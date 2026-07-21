#pragma once
#include "tbx/utils/api.h"
#include <coroutine>
#include <exception>
#include <utility>
#include <variant>

namespace tbx
{
    /// @brief
    /// Purpose: Shared promise plumbing for Task<T>: lazy start plus symmetric transfer to the
    /// awaiting coroutine on completion. Not for direct use — Task's promise types derive it.
    struct TBX_API TaskFinalAwaiter
    {
        bool await_ready() noexcept
        {
            return false;
        }

        template <typename Promise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) noexcept
        {
            return handle.promise().continuation;
        }

        void await_resume() noexcept {}
    };

    /// @brief
    /// Purpose: Base state every Task promise carries: who to resume when the task finishes.
    struct TBX_API TaskPromiseBase
    {
        std::coroutine_handle<> continuation = std::noop_coroutine();

        std::suspend_always initial_suspend() noexcept
        {
            return {};
        }

        TaskFinalAwaiter final_suspend() noexcept
        {
            return {};
        }
    };

    /// @brief
    /// Purpose: The engine-wide async currency — a lazy coroutine that starts when awaited.
    /// @details
    /// Ownership: Owns its coroutine frame; move-only. Thread Safety: Completion resumes the
    /// awaiter via symmetric transfer on whatever thread the task finished on — hop explicitly
    /// with `co_await jobs.on_main()` / `co_await jobs.on_worker()` when the destination matters.
    template <typename T>
    class [[nodiscard]] Task final
    {
      public:
        /// @brief
        /// Purpose: Coroutine promise: stores the result or the escaped exception.
        struct Promise : TaskPromiseBase
        {
            std::variant<std::monostate, T, std::exception_ptr> result;

            Task get_return_object()
            {
                return Task(std::coroutine_handle<Promise>::from_promise(*this));
            }

            void return_value(T value)
            {
                result.template emplace<1>(std::move(value));
            }

            void unhandled_exception()
            {
                result.template emplace<2>(std::current_exception());
            }
        };

        using promise_type = Promise; // the name the coroutine machinery looks for

      public:
        Task() = default;

        ~Task()
        {
            if (_handle)
                _handle.destroy();
        }

      public:
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        Task(Task&& other) noexcept
            : _handle(std::exchange(other._handle, nullptr))
        {
        }

        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                    _handle.destroy();
                _handle = std::exchange(other._handle, nullptr);
            }
            return *this;
        }

      public:
        /// @brief
        /// Purpose: Awaiting starts the task; resumes the awaiter with the result (or rethrows).
        auto operator co_await() &&
        {
            struct Awaiter
            {
                std::coroutine_handle<Promise> handle;

                bool await_ready()
                {
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting)
                {
                    handle.promise().continuation = awaiting;
                    return handle;
                }

                T await_resume()
                {
                    auto& result = handle.promise().result;
                    if (result.index() == 2)
                        std::rethrow_exception(std::get<2>(result));
                    return std::move(std::get<1>(result));
                }
            };
            return Awaiter(_handle);
        }

        /// @brief
        /// Purpose: Reports whether this handle still owns a coroutine (false after move).
        bool is_valid() const
        {
            return _handle != nullptr;
        }

      private:
        explicit Task(std::coroutine_handle<Promise> handle)
            : _handle(handle)
        {
        }

      private:
        std::coroutine_handle<Promise> _handle = nullptr;
    };

    /// @brief
    /// Purpose: Task specialization for coroutines that produce no value.
    template <>
    class [[nodiscard]] Task<void> final
    {
      public:
        /// @brief
        /// Purpose: Coroutine promise: records only an escaped exception.
        struct Promise : TaskPromiseBase
        {
            std::exception_ptr error;

            Task get_return_object()
            {
                return Task(std::coroutine_handle<Promise>::from_promise(*this));
            }

            void return_void() {}

            void unhandled_exception()
            {
                error = std::current_exception();
            }
        };

        using promise_type = Promise; // the name the coroutine machinery looks for

      public:
        Task() = default;

        ~Task()
        {
            if (_handle)
                _handle.destroy();
        }

      public:
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        Task(Task&& other) noexcept
            : _handle(std::exchange(other._handle, nullptr))
        {
        }

        Task& operator=(Task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                    _handle.destroy();
                _handle = std::exchange(other._handle, nullptr);
            }
            return *this;
        }

      public:
        /// @brief
        /// Purpose: Awaiting starts the task; resumes the awaiter when done (or rethrows).
        auto operator co_await() &&
        {
            struct Awaiter
            {
                std::coroutine_handle<Promise> handle;

                bool await_ready()
                {
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting)
                {
                    handle.promise().continuation = awaiting;
                    return handle;
                }

                void await_resume()
                {
                    if (handle.promise().error)
                        std::rethrow_exception(handle.promise().error);
                }
            };
            return Awaiter(_handle);
        }

        /// @brief
        /// Purpose: Reports whether this handle still owns a coroutine (false after move).
        bool is_valid() const
        {
            return _handle != nullptr;
        }

      private:
        explicit Task(std::coroutine_handle<Promise> handle)
            : _handle(handle)
        {
        }

      private:
        std::coroutine_handle<Promise> _handle = nullptr;
    };
}
