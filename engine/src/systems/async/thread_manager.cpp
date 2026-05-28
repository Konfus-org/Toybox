#include "tbx/systems/async/thread_manager.h"
#include <condition_variable>
#include <thread>
#ifdef TBX_PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #include <pthread.h>
#endif

namespace tbx
{
    class ThreadLane final
    {
      public:
        using Task = std::move_only_function<void()>;

      public:
        ThreadLane(std::string lane_name);
        ~ThreadLane() noexcept;

      public:
        ThreadLane(const ThreadLane&) = delete;
        ThreadLane& operator=(const ThreadLane&) = delete;
        ThreadLane(ThreadLane&&) = delete;
        ThreadLane& operator=(ThreadLane&&) = delete;

      public:
        void post(Task&& task);
        void stop();

      private:
        void run(std::stop_token stop_token);
        void set_name(const std::string& lane_name);

      private:
        std::string _name = {};
        std::jthread _worker = {};
        std::mutex _queue_mutex = {};
        std::condition_variable _queued_task_signal = {};
        std::deque<Task> _queued_tasks = {};
        bool _accepting_tasks = true;
    };

    struct ThreadManager::State
    {
        mutable std::mutex lanes_mutex = {};
        std::unordered_map<std::string, std::shared_ptr<ThreadLane>> lanes = {};
    };

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

    ThreadManager::ThreadManager()
        : _state(std::make_unique<State>())
    {
    }

    ThreadManager::~ThreadManager() noexcept
    {
        stop_all();
    }

    bool ThreadManager::try_create_lane(std::string_view lane_name)
    {
        if (lane_name.empty())
            return false;

        auto lock = std::scoped_lock(_state->lanes_mutex);
        auto lane_key = std::string(lane_name);
        if (_state->lanes.contains(lane_key))
            return false;

        auto lane = std::make_shared<ThreadLane>(lane_key);
        _state->lanes.emplace(std::move(lane_key), std::move(lane));
        return true;
    }

    bool ThreadManager::has_lane(std::string_view lane_name) const
    {
        if (lane_name.empty())
            return false;

        auto lock = std::scoped_lock(_state->lanes_mutex);
        return _state->lanes.contains(std::string(lane_name));
    }

    void ThreadManager::post(std::string_view lane_name, Task&& task)
    {
        if (!task)
            return;

        auto lane = std::shared_ptr<ThreadLane> {};
        {
            auto lock = std::scoped_lock(_state->lanes_mutex);
            auto found_lane = _state->lanes.find(std::string(lane_name));
            if (found_lane != _state->lanes.end())
                lane = found_lane->second;
        }

        if (!lane)
            throw std::runtime_error("ThreadManager lane was not found.");
        lane->post(std::move(task));
    }

    void ThreadManager::stop_lane(std::string_view lane_name)
    {
        if (lane_name.empty())
            return;

        auto lane = std::shared_ptr<ThreadLane> {};
        {
            auto lock = std::scoped_lock(_state->lanes_mutex);
            auto found_lane = _state->lanes.find(std::string(lane_name));
            if (found_lane == _state->lanes.end())
                return;

            lane = std::move(found_lane->second);
            _state->lanes.erase(found_lane);
        }

        if (lane)
            lane->stop();
    }

    void ThreadManager::stop_all()
    {
        while (true)
        {
            auto lane = std::shared_ptr<ThreadLane> {};
            {
                auto lock = std::scoped_lock(_state->lanes_mutex);
                auto lane_entry = _state->lanes.begin();
                if (lane_entry == _state->lanes.end())
                    return;

                lane = std::move(lane_entry->second);
                _state->lanes.erase(lane_entry);
            }

            if (lane)
                lane->stop();
        }
    }

    size ThreadManager::get_lane_count() const
    {
        auto lock = std::scoped_lock(_state->lanes_mutex);
        return _state->lanes.size();
    }
}
