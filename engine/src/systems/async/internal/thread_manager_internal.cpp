#include "systems/async/internal/thread_manager_internal.h"
#include <stdexcept>
#include <utility>
#ifdef TBX_PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #include <pthread.h>
#endif

namespace tbx::internal
{
    ThreadLane::ThreadLane(std::string lane_name)
        : _name(std::move(lane_name))
    {
        _worker = std::jthread(
            [this](std::stop_token stop_token)
            {
                set_name(_name);
                run(stop_token);
            });
    }

    ThreadLane::~ThreadLane() noexcept
    {
        stop();
    }

    void ThreadLane::post(Task&& task)
    {
        if (!task)
            return;

        {
            auto lock = std::scoped_lock(_queue_mutex);
            if (!_accepting_tasks)
                throw std::runtime_error("Cannot post to a stopped ThreadManager lane.");
            _queued_tasks.push_back(std::move(task));
        }

        _queued_task_signal.notify_one();
    }

    void ThreadLane::stop()
    {
        {
            auto lock = std::scoped_lock(_queue_mutex);
            if (!_accepting_tasks && !_worker.joinable())
                return;

            _accepting_tasks = false;
        }

        _queued_task_signal.notify_all();
        if (_worker.joinable())
        {
            _worker.request_stop();
            _queued_task_signal.notify_all();
            _worker.join();
        }
    }

    void ThreadLane::run(std::stop_token stop_token)
    {
        while (true)
        {
            auto task = Task {};
            {
                auto lock = std::unique_lock(_queue_mutex);
                _queued_task_signal.wait(
                    lock,
                    [this, stop_token]()
                    {
                        return stop_token.stop_requested() || !_queued_tasks.empty()
                               || !_accepting_tasks;
                    });

                if (_queued_tasks.empty())
                {
                    if (stop_token.stop_requested() || !_accepting_tasks)
                        return;
                    continue;
                }

                task = std::move(_queued_tasks.front());
                _queued_tasks.pop_front();
            }

            try
            {
                task();
            }
            catch (...)
            {
                // Fire-and-forget tasks have no return channel for exceptions.
            }
        }
    }

    void ThreadLane::set_name(const std::string& lane_name)
    {
#if defined(TBX_PLATFORM_WINDOWS)
        auto wide_name = std::wstring(lane_name.begin(), lane_name.end());
        SetThreadDescription(_worker.native_handle(), wide_name.c_str());
#elif defined(TBX_PLATFORM_LINUX)
        auto capped_name = lane_name.substr(0, 15);
        pthread_setname_np(_worker.native_handle(), capped_name.c_str());
#elif defined(TBX_PLATFORM_MACOS)
        pthread_setname_np(lane_name.c_str());
#endif
    }
}
