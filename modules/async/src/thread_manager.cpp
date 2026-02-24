#include "tbx/async/thread_manager.h"
#include <stdexcept>
#include <vector>

namespace tbx
{
    ThreadManager::~ThreadManager() noexcept
    {
        stop_all();
    }

    bool ThreadManager::try_create_lane(std::string_view lane_name)
    {
        if (lane_name.empty())
            return false;

        auto lock = std::scoped_lock(_lanes_mutex);
        auto lane_key = std::string(lane_name);
        if (_lanes.contains(lane_key))
            return false;

        _lanes.emplace(lane_key, std::make_shared<ThreadLane>(lane_key));
        return true;
    }

    bool ThreadManager::has_lane(std::string_view lane_name) const
    {
        if (lane_name.empty())
            return false;

        auto lock = std::scoped_lock(_lanes_mutex);
        return _lanes.contains(std::string(lane_name));
    }

    void ThreadManager::post(std::string_view lane_name, Task&& task)
    {
        if (!task)
            return;

        auto lane = get_lane(lane_name);
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
            auto lock = std::scoped_lock(_lanes_mutex);
            auto found_lane = _lanes.find(std::string(lane_name));
            if (found_lane == _lanes.end())
                return;

            lane = std::move(found_lane->second);
            _lanes.erase(found_lane);
        }

        if (lane)
            lane->stop();
    }

    void ThreadManager::stop_all()
    {
        auto lanes = std::vector<std::shared_ptr<ThreadLane>> {};
        {
            auto lock = std::scoped_lock(_lanes_mutex);
            lanes.reserve(_lanes.size());
            for (auto& lane_entry : _lanes)
                lanes.push_back(std::move(lane_entry.second));
            _lanes.clear();
        }

        for (auto& lane : lanes)
            if (lane)
                lane->stop();
    }

    std::size_t ThreadManager::get_lane_count() const
    {
        auto lock = std::scoped_lock(_lanes_mutex);
        return _lanes.size();
    }

    ThreadManager::ThreadLane::ThreadLane(std::string lane_name)
        : _name(std::move(lane_name))
    {
        _worker = std::jthread(
            [this](std::stop_token stop_token)
            {
                run(stop_token);
            });
    }

    ThreadManager::ThreadLane::~ThreadLane() noexcept
    {
        stop();
    }

    void ThreadManager::ThreadLane::post(Task&& task)
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

    void ThreadManager::ThreadLane::stop()
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

    void ThreadManager::ThreadLane::run(std::stop_token stop_token)
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

    std::shared_ptr<ThreadManager::ThreadLane> ThreadManager::get_lane(
        std::string_view lane_name) const
    {
        if (lane_name.empty())
            return nullptr;

        auto lock = std::scoped_lock(_lanes_mutex);
        auto found_lane = _lanes.find(std::string(lane_name));
        if (found_lane == _lanes.end())
            return nullptr;

        return found_lane->second;
    }
}
