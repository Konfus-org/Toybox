#include "tbx/systems/async/thread_manager.h"
#include "systems/async/internal/thread_manager_internal.h"
#include <stdexcept>
#include <utility>
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

        auto lane = std::make_shared<internal::ThreadLane>(lane_key);
        _lanes.emplace(std::move(lane_key), std::move(lane));
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

        auto lane = std::shared_ptr<internal::ThreadLane> {};
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
        auto lanes = std::vector<std::shared_ptr<internal::ThreadLane>> {};
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

    size ThreadManager::get_lane_count() const
    {
        auto lock = std::scoped_lock(_lanes_mutex);
        return _lanes.size();
    }

    std::shared_ptr<internal::ThreadLane> ThreadManager::get_lane(std::string_view lane_name) const
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
