#include "tbx/systems/async/thread_manager.h"
#include "systems/async/internal/thread_manager_internal.h"
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx
{
    struct ThreadManager::Impl
    {
        mutable std::mutex lanes_mutex = {};
        std::unordered_map<std::string, std::shared_ptr<internal::ThreadLane>> lanes = {};
    };

    ThreadManager::ThreadManager()
        : _impl(std::make_unique<Impl>())
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

        auto lock = std::scoped_lock(_impl->lanes_mutex);
        auto lane_key = std::string(lane_name);
        if (_impl->lanes.contains(lane_key))
            return false;

        auto lane = std::make_shared<internal::ThreadLane>(lane_key);
        _impl->lanes.emplace(std::move(lane_key), std::move(lane));
        return true;
    }

    bool ThreadManager::has_lane(std::string_view lane_name) const
    {
        if (lane_name.empty())
            return false;

        auto lock = std::scoped_lock(_impl->lanes_mutex);
        return _impl->lanes.contains(std::string(lane_name));
    }

    void ThreadManager::post(std::string_view lane_name, Task&& task)
    {
        if (!task)
            return;

        auto lane = std::shared_ptr<internal::ThreadLane> {};
        {
            auto lock = std::scoped_lock(_impl->lanes_mutex);
            auto found_lane = _impl->lanes.find(std::string(lane_name));
            if (found_lane != _impl->lanes.end())
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

        auto lane = std::shared_ptr<internal::ThreadLane> {};
        {
            auto lock = std::scoped_lock(_impl->lanes_mutex);
            auto found_lane = _impl->lanes.find(std::string(lane_name));
            if (found_lane == _impl->lanes.end())
                return;

            lane = std::move(found_lane->second);
            _impl->lanes.erase(found_lane);
        }

        if (lane)
            lane->stop();
    }

    void ThreadManager::stop_all()
    {
        while (true)
        {
            auto lane = std::shared_ptr<internal::ThreadLane> {};
            {
                auto lock = std::scoped_lock(_impl->lanes_mutex);
                auto lane_entry = _impl->lanes.begin();
                if (lane_entry == _impl->lanes.end())
                    return;

                lane = std::move(lane_entry->second);
                _impl->lanes.erase(lane_entry);
            }

            if (lane)
                lane->stop();
        }
    }

    size ThreadManager::get_lane_count() const
    {
        auto lock = std::scoped_lock(_impl->lanes_mutex);
        return _impl->lanes.size();
    }
}
