#pragma once
#include "tbx/systems/graphics/resource_manager.h"
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace tbx
{
    using RenderingResourceCollection = std::vector<uint>;

    class RenderingResourceTracker final
    {
      public:
        RenderingResourceTracker() = default;
        ~RenderingResourceTracker() = default;

      public:
        RenderingResourceTracker(const RenderingResourceTracker&) = delete;
        RenderingResourceTracker& operator=(const RenderingResourceTracker&) = delete;
        RenderingResourceTracker(RenderingResourceTracker&&) noexcept = default;
        RenderingResourceTracker& operator=(RenderingResourceTracker&&) noexcept = default;

      public:
        float get_time_alive(const uint resource) const
        {
            const auto iterator = _time_alive.find(resource);
            return iterator == _time_alive.end() ? 0.0F : iterator->second;
        }

        const RenderingResourceCollection& get_tracked_resources() const
        {
            return _tracked_resources;
        }

        bool is_tracked(const uint resource) const
        {
            return _time_alive.contains(resource);
        }

        void track(const uint resource)
        {
            if (resource == 0U)
                return;
            if (!is_tracked(resource))
                _tracked_resources.push_back(resource);
            _time_alive[resource] = 0.0F;
        }

        void untrack(const uint resource)
        {
            _time_alive.erase(resource);
            const auto iterator =
                std::remove(_tracked_resources.begin(), _tracked_resources.end(), resource);
            _tracked_resources.erase(iterator, _tracked_resources.end());
        }

        void update(const DeltaTime delta)
        {
            const auto seconds = static_cast<float>(delta.seconds);
            for (auto& entry : _time_alive)
                entry.second += seconds;
        }

      private:
        std::unordered_map<uint, float> _time_alive = {};
        RenderingResourceCollection _tracked_resources = {};
    };
}
